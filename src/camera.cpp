/*
	Cam Control - made by sensin

	Drives the game's OWN camera. Nothing is created and nothing is replaced:
	the follow camera, its collision, aiming, cover and driving logic all stay
	the game's, which is what keeps mouse look, walking and driving normal.

	Two layers, applied every tick:

	  1. natives
	       SET_FOLLOW_*_CAM_VIEW_MODE        - the POV (third / first person)
	       SET_FOLLOW_VEHICLE_CAM_ZOOM_LEVEL - vehicle follow distance
	       _CLAMP_GAMEPLAY_CAM_PITCH         - how far the camera may tilt

	  2. camera metadata  (camerameta.cpp)
	       fov / follow distance / pivot. The game has no native for these, so
	       they are written into the live metadata object of the camera that is
	       on screen. That layer disables itself if it cannot find what it needs.
*/

#include "camera.h"
#include "camerameta.h"
#include "log.h"

#include "natives.h"

#include <windows.h>
#include <cmath>
#include <cstring>

namespace
{
	// Last view mode we forced, so a change is logged once instead of every
	// frame. -1 means "nothing forced yet".
	int g_lastPedView = -1;
	int g_lastVehView = -1;

	// State of one forced view mode. `took` records that the game actually
	// reported the mode we asked for, which is what makes a later change
	// recognisable as somebody else's doing.
	struct ViewForce
	{
		int want = -1;      // mode being asserted, -1 = nothing asserted
		bool took = false;  // the game has reported that mode at least once
	};

	ViewForce g_forcePed;
	ViewForce g_forceVeh;

	// Set when the mod stopped forcing a view mode because the player took
	// over. Read by the caller so the change can be saved.
	bool g_forceReleased = false;

	// The vehicle view mode the player actually wants, learned while there is
	// no hazard. Only used to undo the game's own water / wreck switch, so a
	// deliberate V press (which happens outside a hazard) is never fought.
	int g_wantVehView = -1;

	// True while a shake started by this mod is running, so turning the slider
	// back to 0 stops only our own shake and never the game's.
	bool g_shakeStarted = false;

	// FOV cross-check state (reset on Shutdown/ResetLook).
	int g_mismatchFrames = 0;
	bool g_mismatchReported = false;

	float Clamp(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	// Applies a view-mode setting. vehicle == false -> on-foot camera.
	//
	// The mode is asserted only until the game accepts it, and then the mod
	// stops writing. Writing it every tick is what made V look dead: the game
	// switches the camera, the next tick forced it straight back, so the player
	// saw nothing happen. Once `took` is set, a change away from the forced
	// mode is treated as the player's own doing (V, or the game switching by
	// itself) and the force is released instead of being re-applied.
	void ApplyViewMode(int &setting, ViewForce &f, bool vehicle, bool allowRelease)
	{
		int want = -1;
		if (setting == VIEWMODE_THIRD)
			want = 0;
		else if (setting == VIEWMODE_FIRST)
			want = 4;

		if (want < 0)
		{
			f.want = -1;
			f.took = false;
			return;
		}

		if (f.want != want)
		{
			f.want = want;
			f.took = false;
		}

		const int cur = vehicle ? CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE()
		                        : CAM::GET_FOLLOW_PED_CAM_VIEW_MODE();

		if (cur == want)
		{
			f.took = true;
			return;
		}

		// Hazards keep the vehicle camera on the mode the player chose before
		// the hazard started, so those frames are not a player decision.
		if (f.took && !allowRelease)
			return;

		if (f.took)
		{
			Log::Line("camera: %s view mode moved to %d - releasing the forced mode, "
				"the player is in charge again", vehicle ? "vehicle" : "on-foot", cur);
			setting = VIEWMODE_DONT_TOUCH;
			(vehicle ? g_lastVehView : g_lastPedView) = -1;
			f.want = -1;
			f.took = false;
			g_forceReleased = true;
			return;
		}

		if (vehicle)
			CAM::SET_FOLLOW_VEHICLE_CAM_VIEW_MODE(want);
		else
			CAM::SET_FOLLOW_PED_CAM_VIEW_MODE(want);

		int &lastForced = vehicle ? g_lastVehView : g_lastPedView;
		if (lastForced != want)
		{
			Log::Line("camera: forced %s view mode %d",
				vehicle ? "vehicle" : "on-foot", want);
			lastForced = want;
		}
	}
}

namespace CameraControl
{
	const char *StatusText()
	{
		return CamMeta::Available()
			? "game camera + natives + metadata"
			: "game camera + natives (metadata layer unavailable)";
	}

	void ResetLook()
	{
		g_lastPedView = -1;
		g_lastVehView = -1;
		g_wantVehView = -1;
		g_forcePed = ViewForce();
		g_forceVeh = ViewForce();
		g_forceReleased = false;
		g_shakeStarted = false;
		g_mismatchFrames = 0;
		g_mismatchReported = false;
		CamMeta::Reset();
	}

	bool ConsumeForceReleased()
	{
		const bool was = g_forceReleased;
		g_forceReleased = false;
		return was;
	}

	void Shutdown()
	{
		if (g_shakeStarted)
		{
			CAM::STOP_GAMEPLAY_CAM_SHAKING(TRUE);
			g_shakeStarted = false;
		}

		CamMeta::RestoreAll();
		ResetLook();
	}

	// Works out which camera the player is on right now and dumps its metadata.
	// Kept separate from Tick() so the dump works even while the mod is disabled -
	// a stale context would dump the wrong camera.
	void DumpMetadata()
	{
		Ped ped = PLAYER::PLAYER_PED_ID();
		if (ped == 0 || !ENTITY::DOES_ENTITY_EXIST(ped))
		{
			Log::Line("dump: no player ped yet");
			return;
		}

		Vehicle veh = 0;
		bool inVeh = false;
		if (PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE))
		{
			Vehicle v = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
			if (v != 0 && ENTITY::DOES_ENTITY_EXIST(v))
			{
				veh = v;
				inVeh = true;
			}
		}

		const bool firstPerson = inVeh
			? (CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE() == 4)
			: (CAM::GET_FOLLOW_PED_CAM_VIEW_MODE() == 4);

		CamMeta::Dump(ped, veh, inVeh, firstPerson);
		Log::Line("dump: written - see CamControl.log");
	}

	void Tick(Settings &s, bool menuOpen)
	{
		// The menu no longer changes how the camera is driven: the game owns the
		// camera and keeps owning it while the menu is open.
		(void)menuOpen;

		if (!s.enabled)
		{
			Shutdown();
			return;
		}

		Ped ped = PLAYER::PLAYER_PED_ID();
		if (ped == 0 || !ENTITY::DOES_ENTITY_EXIST(ped) || PED::IS_PED_DEAD_OR_DYING(ped, TRUE))
		{
			if (g_shakeStarted)
			{
				CAM::STOP_GAMEPLAY_CAM_SHAKING(TRUE);
				g_shakeStarted = false;
			}
			return;
		}

		Vehicle veh = 0;
		bool inVeh = false;
		if (PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE))
		{
			Vehicle v = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
			if (v != 0 && ENTITY::DOES_ENTITY_EXIST(v))
			{
				veh = v;
				inVeh = true;
			}
		}

		// Worked out before the view mode is applied: while the vehicle is in
		// deep water or wrecked/undriveable, a change of view mode is the game's
		// doing and must not be mistaken for the player pressing V. Boats are
		// excluded from the water check so V still works on water.
		const bool wrecked = inVeh && (ENTITY::IS_ENTITY_DEAD(veh) || !VEHICLE::IS_VEHICLE_DRIVEABLE(veh, FALSE));
		const bool inDeepWater = inVeh &&
			!VEHICLE::IS_THIS_MODEL_A_BOAT(ENTITY::GET_ENTITY_MODEL(veh)) &&
			ENTITY::IS_ENTITY_IN_WATER(veh) &&
			ENTITY::GET_ENTITY_SUBMERGED_LEVEL(veh) > 0.35f;
		const bool hazard = inVeh &&
			((s.vehFpNoSwapInWater && inDeepWater) ||
			 (s.vehFpNoSwapOnDestroyed && wrecked));

		// ---- POV (only the camera we are actually using) -----------------
		if (inVeh)
			ApplyViewMode(s.vehViewMode, g_forceVeh, true, !hazard);
		else
			ApplyViewMode(s.pedViewMode, g_forcePed, false, true);

		// ---- keep first person when the game would drop it ---------------
		// Entering deep water and wrecking the vehicle both make the game force
		// the camera back to third person. These two options undo that for the
		// duration of the hazard, by re-asserting the view mode the player was
		// already using. Outside the hazard the game is left in charge, so
		// pressing V still does what the player expects.
		if (inVeh)
		{
			const int cur = CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE();

			if (!hazard)
				g_wantVehView = cur;                                  // player is in charge
			else if ((g_wantVehView == 4 || s.vehViewMode == VIEWMODE_FIRST) && cur != 4)
				CAM::SET_FOLLOW_VEHICLE_CAM_VIEW_MODE(4);              // undo the game's switch
		}
		else
		{
			g_wantVehView = -1;
		}

		const bool firstPerson = inVeh
			? (CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE() == 4)
			: (CAM::GET_FOLLOW_PED_CAM_VIEW_MODE() == 4);

		const bool inCutscene = (CUTSCENE::IS_CUTSCENE_ACTIVE() != 0);

		// ---- unarmed reticle in on-foot first person ---------------------
		// Metadata ShouldDisplayReticule alone does not force the HUD reticle
		// to draw when unarmed, so drive HUD_RETICLE (14) directly here too.
		if (!inVeh && firstPerson && !inCutscene && s.pedFpReticle != 0)
		{
			const Hash weapon = WEAPON::GET_SELECTED_PED_WEAPON(ped);
			if (weapon == 0xA2719263u) // WEAPON_UNARMED
			{
				if (s.pedFpReticle == 1)
					UI::SHOW_HUD_COMPONENT_THIS_FRAME(14);
				else if (s.pedFpReticle == 2)
					UI::HIDE_HUD_COMPONENT_THIS_FRAME(14);
			}
		}

		// ---- camera shake ------------------------------------------------
		// GTA applies its own shake values after the script tick, so merely
		// scaling the current shake from here is overwritten before the frame
		// is drawn - which made the old slider look dead. The mod therefore
		// starts a shake of its own and keeps it running at the requested
		// strength. 0 still means "off": it stops only a shake this mod
		// started, so the game's own shakes are never touched.
		const float shake = inCutscene ? 0.0f : (inVeh ? s.shakeInVehicle : s.shakeOnFoot);
		if (shake > 0.0f)
		{
			// 100 % is a clearly visible but still usable amount; the slider
			// allows up to 300 %.
			const float amplitude = Clamp(shake / 100.0f, 0.02f, 3.0f);

			// Re-armed only when nothing is shaking, so the shake plays out
			// and then continues instead of being restarted every frame.
			if (!CAM::IS_GAMEPLAY_CAM_SHAKING())
			{
				char name[24];
				strcpy_s(name, "HAND_SHAKE");
				CAM::SHAKE_GAMEPLAY_CAM(name, amplitude);
			}

			CAM::SET_GAMEPLAY_CAM_SHAKE_AMPLITUDE(amplitude);
			g_shakeStarted = true;
		}
		else if (g_shakeStarted)
		{
			CAM::STOP_GAMEPLAY_CAM_SHAKING(TRUE);
			g_shakeStarted = false;
		}

		// ---- vehicle follow zoom (native) --------------------------------
		// 0 = leave alone; 1..3 map to 0 (Near), 1 (Medium), 2 (Far).
		// Older configs with 4 are clamped to 2 (Far) rather than passing 4,
		// which GTA interprets as first-person hood/interior view.
		if (inVeh && !firstPerson && s.vehTpZoom >= 1.0f)
		{
			int zoom = (int)Clamp(s.vehTpZoom, 1.0f, 3.0f) - 1;
			CAM::SET_FOLLOW_VEHICLE_CAM_ZOOM_LEVEL(zoom);
		}

		// ---- tilt limits (native) ----------------------------------------
		// One bound per direction. A side left at 0 keeps a bound wide enough to
		// be the game's own, so setting only "up" does not secretly pin the
		// camera flat, which is what a naive (-0, up) clamp would do.
		if (!inCutscene)
		{
			const float tiltUp = inVeh
				? (firstPerson ? s.vehFpTiltUp : s.vehTpTiltUp)
				: (firstPerson ? s.pedFpTiltUp : s.pedTpTiltUp);
			const float tiltDown = inVeh
				? (firstPerson ? s.vehFpTiltDown : s.vehTpTiltDown)
				: (firstPerson ? s.pedFpTiltDown : s.pedTpTiltDown);

			if (tiltUp > 0.5f || tiltDown > 0.5f)
				CAM::_CLAMP_GAMEPLAY_CAM_PITCH(tiltDown > 0.5f ? -tiltDown : -89.0f,
				                               tiltUp > 0.5f ? tiltUp : 89.0f);
		}

		// ---- fov / follow distance / pivot (camera metadata) -------------
		CamMeta::Tick(s, ped, veh, inVeh, firstPerson);

		// ---- fov cross-check ---------------------------------------------
		// The metadata layer writes a field of view; the game reports the one
		// it is really rendering with. When those disagree for a couple of
		// seconds, something else is driving the camera - another mod's menu
		// is the usual suspect - and that mismatch is what makes particles,
		// light effects and world detail stop being drawn near the edges of
		// the screen. Logged on change only, so the log itself says whether
		// this mod's FOV is even in effect at the moment the picture breaks.
		float wantFov = 0.0f;
		unsigned int wantHash = 0;
		if (!inCutscene && CamMeta::LastWriteInfo(wantFov, wantHash))
		{
			const float gameFov = CAM::GET_GAMEPLAY_CAM_FOV();
			const bool mismatch = fabsf(gameFov - wantFov) > 2.0f;

			if (mismatch)
				g_mismatchFrames++;
			else
				g_mismatchFrames = 0;

			if (mismatch && g_mismatchFrames == 120 && !g_mismatchReported)
			{
				g_mismatchReported = true;
				Log::Line("camera: the game renders fov %.2f but the metadata written to "
					"0x%X asks for %.2f - something else is overriding the camera",
					gameFov, wantHash, wantFov);
			}
			else if (!mismatch && g_mismatchReported)
			{
				g_mismatchReported = false;
				Log::Line("camera: fov agrees again at %.2f", gameFov);
			}
		}

		// ---- dev probe ---------------------------------------------------
		// Only ever writes while the menu is open, so the player can watch the
		// camera react and close the menu to stop it.
		CamMeta::Probe(s, ped, veh, inVeh, firstPerson, menuOpen);
		CamMeta::Sweep(s, ped, veh, inVeh, firstPerson, menuOpen);
	}
}
