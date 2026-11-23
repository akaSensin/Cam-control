/*
	Cam Control - camera control mod for GTA V (ScriptHookV, offline)
	made by sensin

	Settings model. Everything in this struct is persisted to CamControl.ini
	so the mod remembers the configuration between game sessions.
*/

#pragma once

// ---------------------------------------------------------------------------
// View modes
// ---------------------------------------------------------------------------

// Index into the "On-Foot View" / "Vehicle View" option rows.
enum ViewModeSetting
{
	VIEWMODE_DONT_TOUCH = 0, // index 0 - leave the game setting alone
	VIEWMODE_THIRD = 1,      // index 1 - force third person
	VIEWMODE_FIRST = 2       // index 2 - force first person
};

// ---------------------------------------------------------------------------
// Cam Control drives the game's OWN camera: nothing is created or replaced,
// so walking, aiming, cover, driving, mouse look and camera collision all stay
// exactly as the game does them. Two layers are used on top of it:
//
//   1. Public natives  - view modes, vehicle zoom, pitch limits.
//   2. Camera metadata - FOV / follow distance / pivot, the same values the
//      game loads from its own camera .meta files. There is no native for
//      these, so they are written straight into the live metadata object.
//      See camerameta.cpp; it degrades to a no-op if it cannot find them.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Theme presets (index into the "Theme" option row)
// ---------------------------------------------------------------------------

enum ThemePreset
{
	THEME_CUSTOM = 0,
	THEME_MONOCHROME = 1,
	THEME_AMBER = 2,
	THEME_EMERALD = 3,
	THEME_CYAN = 4,
	THEME_CRIMSON = 5,
	THEME_VIOLET = 6,
	THEME_BLUE = 7,
	THEME_COUNT = 8
};

// Packs a preset into RGB. Index THEME_CUSTOM returns false: keep the user's
// own R/G/B sliders untouched.
inline bool ThemePresetColor(int preset, int &r, int &g, int &b)
{
	switch (preset)
	{
	case THEME_MONOCHROME: r = 255; g = 255; b = 255; return true;
	case THEME_AMBER:      r = 255; g = 176; b = 0;   return true;
	case THEME_EMERALD:    r = 0;   g = 230; b = 118; return true;
	case THEME_CYAN:       r = 0;   g = 229; b = 255; return true;
	case THEME_CRIMSON:    r = 255; g = 45;  b = 85;  return true;
	case THEME_VIOLET:     r = 170; g = 90;  b = 255; return true;
	case THEME_BLUE:       r = 41;  g = 121; b = 255; return true;
	default:               r = g = b = 255;           return false;
	}
}

struct Settings
{
	// --- core ---
	bool enabled = true;                  // master switch
	bool experimentalMeta = true;         // write the game's camera metadata (FOV etc.)
	bool metaAiming = true;               // ...and keep the FOV applied while aiming

	// --- ui ---
	bool autoSave = true;                 // save the ini whenever a value changes
	bool debugOverlay = false;            // small info panel
	int  language = 0;                    // Lang::Id - see lang.h
	int themePreset = THEME_MONOCHROME;   // accent colour preset
	int themeR = 255;                     // custom accent, 0..255
	int themeG = 255;
	int themeB = 255;

	// -----------------------------------------------------------------------
	// View / POV (native). Applied before the per-camera values below, so the
	// matching section is the one that takes effect.
	// -----------------------------------------------------------------------
	int pedViewMode = VIEWMODE_DONT_TOUCH;
	int vehViewMode = VIEWMODE_DONT_TOUCH;

	// -----------------------------------------------------------------------
	// On foot - third person
	// -----------------------------------------------------------------------
	float pedTpFov = 50.0f;        // camera metadata "fov"
	float pedTpDistance = 1.075f;  // camera metadata "followDistance"
	float pedTpPivotX = 0.0f;      // camera metadata "pivotOffsetX"
	bool  pedTpPitchOverride = false; // override BaseOrbitPitchOffset on foot
	float pedTpDefaultPitch = 0.0f;   // target resting pitch (-45..45 deg)

	// While aiming the game switches to a different camera, and these three
	// tune THAT one. 0 = "same as third person", which is what the mod did
	// before these existed, so an untouched config behaves exactly as before.
	float pedAimFov = 0.0f;        // camera metadata "fov" on the aiming camera
	float pedAimDistance = 0.0f;   // ...its follow distance
	float pedAimPivotX = 0.0f;     // ...its horizontal origin

	// -----------------------------------------------------------------------
	// Tilt limits. One value per direction: 0 = leave that direction alone.
	// Setting only one side works - the other keeps a bound wide enough to be
	// the game's own, so a slider at 0 never pins the camera to the horizon.
	// -----------------------------------------------------------------------
	float pedTpTiltUp = 0.0f;    // native pitch clamp, how far the camera looks up
	float pedTpTiltDown = 0.0f;  // ...and down
	float pedFpTiltUp = 0.0f;
	float pedFpTiltDown = 0.0f;
	float vehTpTiltUp = 0.0f;
	float vehTpTiltDown = 0.0f;
	float vehFpTiltUp = 0.0f;
	float vehFpTiltDown = 0.0f;

	// -----------------------------------------------------------------------
	// On foot - first person
	// -----------------------------------------------------------------------
	float pedFpFov = 50.0f;        // camera metadata "fov" (FPS metadata)
	float pedFpOffsetX = 0.0f;     // camera metadata "AttachRelativeOffset.x"

	// -----------------------------------------------------------------------
	// Vehicle - third person
	// -----------------------------------------------------------------------
	float vehTpFov = 50.0f;
	float vehTpDistance = 1.075f;
	float vehTpPivotX = 0.0f;      // camera metadata "CameraRelativeSideOffset"
	float vehTpPivotScale = 1.0f;
	float vehTpZoom = 0.0f;        // native follow zoom, 0 = leave the game alone

	// -----------------------------------------------------------------------
	// Vehicle - first person
	// -----------------------------------------------------------------------
	float vehFpFov = 50.0f;
	float vehFpOffsetX = 0.0f;     // camera metadata "viewOffsetX"
	float vehFpOffsetZ = 0.0f;     // camera metadata "viewOffsetZ"

	// --- dev: live metadata probe -----------------------------------------
	// Used to find the field offset of an option the mod does not support yet.
	// While the menu is open and this is enabled, the value is written into the
	// active camera's metadata at `probeOffset`, so the effect can be seen on
	// screen straight away. Closing the menu stops it, so nothing persists.
	bool  probeEnabled = false;
	int   probeOffset = 0x30;      // byte offset inside the metadata object
	float probeValue = 50.0f;

	// Automatic field sweep: walks every offset in the metadata object, perturbs
	// it, and records how the camera reacted. Runs only while the menu is open.
	bool  sweepEnabled = false;
	int   sweepFrom = 0;           // offset the sweep starts (or resumes) at
	// Idle animations move the character (and the camera with him), which both
	// pollutes the sweep's measurements and forces the player to sit on a key to
	// interrupt them. Off = leave the game's animations alone.
	bool  sweepDisableIdleAnims = true;

	// -----------------------------------------------------------------------
	// Weapons - the aiming camera the game picks for whatever is in your hands.
	//
	// These are separate metadata objects, one per weapon class, and each has
	// its own FOV. 0 means "leave the game's own value alone", the same
	// convention the pitch limits and the vehicle zoom already use, so nothing
	// is written until a slider is actually moved.
	//
	// Weapons the game gives no camera of their own - pistols, SMG, rifles, MG
	// and everything else - all fall back to the plain aiming camera, which is
	// the "Default" row. Those cannot be split apart: there is only one camera
	// behind them, and it is the one the Default slider writes.
	//
	// The sniper entries are a different metadata type from the rest, and their
	// FOV sits at a different offset - see camerameta.cpp.
	// -----------------------------------------------------------------------
	float weaponFovDefault = 0.0f;   // DEFAULT_THIRD_PERSON_PED_AIM_CAMERA
	float weaponFovHip = 0.0f;       // HIP_AIM_CAMERA        launcher / extinguisher / petrol can
	float weaponFovRpg = 0.0f;       // RPG_AIM_CAMERA        rockets and stingers
	float weaponFovShotgun = 0.0f;   // SHOTGUN_AIM_CAMERA
	float weaponFovThrown = 0.0f;    // THROWN_AIM_CAMERA     grenade, molotov, flare, ball
	float weaponFovMelee = 0.0f;     // MELEE_AIM_CAMERA      knife, fist, animals
	float weaponFovDigiscan = 0.0f;  // DIGISCANNER_AIM_CAMERA
	float weaponFovJerry = 0.0f;     // JERRY_CAN_RUN_AND_GUN_CAMERA
	float weaponFovSniper = 0.0f;    // SNIPER_AIM_CAMERA     scoped
	float weaponFovSniperLow = 0.0f; // SNIPER_LOW_ZOOM_AIM_CAMERA
	float weaponFovScopeOther = 0.0f;// remaining 7 camFirstPersonPedAimCameraMetadata

	// -----------------------------------------------------------------------
	// Extra cameras (Parachute, Vehicle Aim / Drive-By)
	// 0 = leave the game's value alone.
	// -----------------------------------------------------------------------
	float parachuteFov = 0.0f;       // camFollowParachuteCameraMetadata (2)
	float parachuteDistance = 0.0f;  // ...follow distance
	float vehAimFov = 0.0f;          // camThirdPersonVehicleAimCameraMetadata (36)

	// --- first person extras ---------------------------------------------
	// The unarmed reticle lives on a bool inside the first person metadata.
	// Three states rather than a toggle, so "leave the game alone" is
	// expressible - a plain toggle would have to pick a side on first run.
	//   0 = do not touch, 1 = always show, 2 = never show
	int pedFpReticle = 0;

	// Field of view used while aiming in first person, which is a separate
	// field from the normal one. 0 = leave the game's value alone.
	float pedFpAimFov = 0.0f;

	// --- camera shake -----------------------------------------------------
	// The mod starts its own gameplay-camera shake and keeps it running at this
	// strength. The game overwrites a shake amplitude set from a script before
	// the frame is drawn, so scaling its shake from here had no visible effect.
	// 0 = off. 100 = a clearly visible shake, up to 300 allowed.
	float shakeOnFoot = 0.0f;
	float shakeInVehicle = 0.0f;

	// --- stop the game dropping first person ------------------------------
	// The game forces the camera out of first person when the vehicle hits
	// deep water, and again when the vehicle is wrecked. These undo that by
	// re-asserting the view mode while the hazard lasts. The mod defers to the
	// player everywhere else, so a deliberate V press still works.
	bool vehFpNoSwapInWater = false;
	bool vehFpNoSwapOnDestroyed = false;

	// --- keys (Win32 virtual-key codes) ---
	// Defaults deliberately avoid the numpad so the menu is usable on tenkeyless
	// and laptop keyboards (e.g. Redragon K552). Arrow keys always navigate as a
	// safety net on top of whatever is bound here.
	int keyPrimary = 0xBD;   // '-' (VK_OEM_MINUS) - main open key
	int keyAlt1 = 0x77;      // VK_F8 - optional extra open key
	int keyAlt2 = 0x00;      // optional extra open key (0 = not bound)
	int keyUp = 0x26;        // VK_UP
	int keyDown = 0x28;      // VK_DOWN
	int keyLeft = 0x25;      // VK_LEFT
	int keyRight = 0x27;     // VK_RIGHT
	int keySelect = 0x0D;    // VK_RETURN
	int keyBack = 0x08;      // VK_BACK
};

// A single flat keymap used by the menu system.
struct KeyMap
{
	int up;
	int down;
	int left;
	int right;
	int select;
	int back;
};

inline KeyMap makeKeyMap(const Settings &s)
{
	KeyMap k;
	k.up = s.keyUp;
	k.down = s.keyDown;
	k.left = s.keyLeft;
	k.right = s.keyRight;
	k.select = s.keySelect;
	k.back = s.keyBack;
	return k;
}
