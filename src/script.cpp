/*
	Cam Control - camera control mod for GTA V (offline, ScriptHookV)
	made by sensin

	Menu pages are declared here, the per tick loop lives in ScriptMain().
*/

#include "script.h"

#include "settings.h"
#include "menu.h"
#include "lang.h"
#include "camera.h"
#include "camerameta.h"
#include "config.h"
#include "keys.h"
#include "keyboard.h"
#include "log.h"
#include "ui.h"
#include "lang.h"
#include "dxtext.h"
#include "updater.h"

#include "main.h"
#include "natives.h"

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define MOD_NAME    "Cam Control"
#define MOD_AUTHOR  "sensin"
#define MOD_VERSION "v0.1"

namespace
{
	const int PAGE_MAIN = 0;
	const int PAGE_PED_TP = 1;   // on foot, third person
	const int PAGE_PED_FP = 2;   // on foot, first person
	const int PAGE_VEH_TP = 3;   // vehicle, third person
	const int PAGE_VEH_FP = 4;   // vehicle, first person
	const int PAGE_THEME = 5;
	const int PAGE_KEYS = 6;
	const int PAGE_SETTINGS = 7;
	const int PAGE_ABOUT = 8;
	const int PAGE_WEAPONS = 9;  // its page is added last, so the numbers above hold
	const int PAGE_EXTRA_CAMS = 10;

	const DWORD SAVE_DEBOUNCE_MS = 700;

	HMODULE g_module = nullptr;
	std::wstring g_dir;
	std::wstring g_iniPath;
	std::wstring g_logPath;

	Settings g_settings;
	MenuSystem g_menu;

	bool g_dirty = false;
	DWORD g_dirtySince = 0;
	bool g_wasOpen = false;
	bool g_wasEnabled = true;
	int g_wasLanguage = 0;

	// English keys on purpose: MenuSystem::Option() runs every entry through
	// Lang::Tr() when the page is built, which is also why these are not
	// translated here (that would translate a translation).
	const std::vector<std::string> VIEW_NAMES{ "Don't touch", "Third Person", "First Person" };
	const std::vector<std::string> THEME_NAMES
	{
		"Custom (RGB below)", "Monochrome", "Amber", "Emerald", "Cyan", "Crimson", "Violet", "Blue"
	};

	// ------------------------------------------------------------------
	// paths / persistence
	// ------------------------------------------------------------------

	bool EndsWithNoCase(const std::wstring &str, const std::wstring &suffix)
	{
		if (str.size() < suffix.size())
			return false;
		return _wcsicmp(str.c_str() + (str.size() - suffix.size()), suffix.c_str()) == 0;
	}

	void ResolvePaths()
	{
		wchar_t buf[MAX_PATH * 2] = { 0 };
		std::wstring baseDir;
		if (g_module && GetModuleFileNameW(g_module, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]))) > 0)
		{
			std::wstring full(buf);
			size_t slash = full.find_last_of(L"\\/");
			baseDir = (slash == std::wstring::npos) ? L"" : full.substr(0, slash + 1);
		}

		// Store all mod files under scripts\Cam Control\ (never in game root).
		if (EndsWithNoCase(baseDir, L"\\scripts\\Cam Control\\") || EndsWithNoCase(baseDir, L"/scripts/Cam Control/"))
		{
			g_dir = baseDir;
		}
		else if (EndsWithNoCase(baseDir, L"\\scripts\\") || EndsWithNoCase(baseDir, L"/scripts/"))
		{
			g_dir = baseDir + L"Cam Control\\";
			CreateDirectoryW(g_dir.c_str(), nullptr);
		}
		else
		{
			const std::wstring scriptsDir = baseDir + L"scripts\\";
			CreateDirectoryW(scriptsDir.c_str(), nullptr);
			g_dir = scriptsDir + L"Cam Control\\";
			CreateDirectoryW(g_dir.c_str(), nullptr);
		}

		g_iniPath = g_dir + L"CamControl.ini";
		g_logPath = g_dir + L"CamControl.log";
	}

	bool CopyTextToClipboard(const std::string &text)
	{
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (!hMem)
		{
			CloseClipboard();
			return false;
		}
		void *dst = GlobalLock(hMem);
		if (!dst)
		{
			GlobalFree(hMem);
			CloseClipboard();
			return false;
		}
		memcpy(dst, text.c_str(), text.size() + 1);
		GlobalUnlock(hMem);
		if (!SetClipboardData(CF_TEXT, hMem))
		{
			GlobalFree(hMem);
			CloseClipboard();
			return false;
		}
		CloseClipboard();
		return true;
	}

	bool ReadTextFromClipboard(std::string &out)
	{
		out.clear();
		if (!OpenClipboard(nullptr))
			return false;
		HANDLE hData = GetClipboardData(CF_TEXT);
		if (hData)
		{
			const char *src = (const char *)GlobalLock(hData);
			if (src)
			{
				out = src;
				GlobalUnlock(hData);
			}
		}
		CloseClipboard();
		return !out.empty();
	}

	void MarkDirty()
	{
		g_dirty = true;
		g_dirtySince = GetTickCount();
	}

	void SaveSettingsNow()
	{
		Config::Save(g_iniPath, g_settings);
		g_dirty = false;
	}

	void ResetKeysToDefault(const Settings &def)
	{
		g_settings.keyPrimary = def.keyPrimary;
		g_settings.keyAlt1 = def.keyAlt1;
		g_settings.keyAlt2 = def.keyAlt2;
		g_settings.keyUp = def.keyUp;
		g_settings.keyDown = def.keyDown;
		g_settings.keyLeft = def.keyLeft;
		g_settings.keyRight = def.keyRight;
		g_settings.keySelect = def.keySelect;
		g_settings.keyBack = def.keyBack;
	}

	// A menu-open key that is also one of the menu's own keys fires both at once:
	// Enter would open the menu AND select, Backspace would close it and go back,
	// an arrow key would open it and move the selection. The bind screen refuses
	// those keys outright, but a hand-edited or older ini can still contain one,
	// so they are cleaned up on load as well.
	void SanitizeOpenKeys()
	{
		const Settings def;

		const int reserved[] =
		{
			VK_RETURN, VK_BACK, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
			g_settings.keySelect, g_settings.keyBack,
			g_settings.keyUp, g_settings.keyDown, g_settings.keyLeft, g_settings.keyRight
		};
		const int reservedCount = (int)(sizeof(reserved) / sizeof(reserved[0]));

		int *openKeys[] = { &g_settings.keyPrimary, &g_settings.keyAlt1, &g_settings.keyAlt2 };
		const int fallback[] = { def.keyPrimary, def.keyAlt1, def.keyAlt2 };

		for (int i = 0; i < 3; i++)
		{
			if (*openKeys[i] == 0)
				continue;

			bool collision = false;
			for (int j = 0; j < reservedCount; j++)
			{
				if (*openKeys[i] == reserved[j])
				{
					collision = true;
					break;
				}
			}
			if (!collision)
				continue;

			// Use the default for this slot, or leave it unbound if even that is
			// taken (only possible if the menu keys were remapped onto the
			// default open key).
			int replacement = 0;
			bool fallbackTaken = false;
			for (int j = 0; j < reservedCount; j++)
			{
				if (fallback[i] == reserved[j])
				{
					fallbackTaken = true;
					break;
				}
			}
			if (!fallbackTaken)
				replacement = fallback[i];

			Log::Line("keys: open key %s is also a menu key - replaced with %s",
				Keys::Name(*openKeys[i]).c_str(), Keys::Name(replacement).c_str());
			*openKeys[i] = replacement;
		}
	}

	// Applying a theme preset copies its RGB into the sliders.
	void ApplyThemePreset()
	{
		int r = 255, g = 255, b = 255;
		if (ThemePresetColor(g_settings.themePreset, r, g, b))
		{
			g_settings.themeR = r;
			g_settings.themeG = g;
			g_settings.themeB = b;
		}
	}

	// ------------------------------------------------------------------
	// per-page reset helpers (each camera page keeps one as its first row)
	// ------------------------------------------------------------------

	void ResetOnFootThirdPerson()
	{
		const Settings d;
		g_settings.pedTpFov = d.pedTpFov;
		g_settings.pedTpDistance = d.pedTpDistance;
		g_settings.pedTpPivotX = d.pedTpPivotX;
		g_settings.pedAimFov = d.pedAimFov;
		g_settings.pedAimDistance = d.pedAimDistance;
		g_settings.pedAimPivotX = d.pedAimPivotX;
		g_settings.pedTpTiltUp = d.pedTpTiltUp;
		g_settings.pedTpTiltDown = d.pedTpTiltDown;
		g_settings.metaAiming = d.metaAiming;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: on-foot third person");
	}

	void ResetOnFootFirstPerson()
	{
		const Settings d;
		g_settings.pedFpFov = d.pedFpFov;
		g_settings.pedFpAimFov = d.pedFpAimFov;
		g_settings.pedFpReticle = d.pedFpReticle;
		g_settings.pedFpTiltUp = d.pedFpTiltUp;
		g_settings.pedFpTiltDown = d.pedFpTiltDown;
		g_settings.shakeOnFoot = d.shakeOnFoot;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: on-foot first person");
	}

	void ResetVehicleThirdPerson()
	{
		const Settings d;
		g_settings.vehTpFov = d.vehTpFov;
		g_settings.vehTpDistance = d.vehTpDistance;
		g_settings.vehTpPivotScale = d.vehTpPivotScale;
		g_settings.vehTpZoom = d.vehTpZoom;
		g_settings.vehTpTiltUp = d.vehTpTiltUp;
		g_settings.vehTpTiltDown = d.vehTpTiltDown;
		g_settings.shakeInVehicle = d.shakeInVehicle;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: vehicle third person");
	}

	void ResetVehicleFirstPerson()
	{
		const Settings d;
		g_settings.vehFpFov = d.vehFpFov;
		g_settings.vehFpOffsetZ = d.vehFpOffsetZ;
		g_settings.vehFpTiltUp = d.vehFpTiltUp;
		g_settings.vehFpTiltDown = d.vehFpTiltDown;
		g_settings.vehFpNoSwapInWater = d.vehFpNoSwapInWater;
		g_settings.vehFpNoSwapOnDestroyed = d.vehFpNoSwapOnDestroyed;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: vehicle first person");
	}

	void ResetWeapons()
	{
		const Settings d;
		g_settings.weaponFovDefault = d.weaponFovDefault;
		g_settings.weaponFovHip = d.weaponFovHip;
		g_settings.weaponFovRpg = d.weaponFovRpg;
		g_settings.weaponFovShotgun = d.weaponFovShotgun;
		g_settings.weaponFovThrown = d.weaponFovThrown;
		g_settings.weaponFovMelee = d.weaponFovMelee;
		g_settings.weaponFovDigiscan = d.weaponFovDigiscan;
		g_settings.weaponFovJerry = d.weaponFovJerry;
		g_settings.weaponFovSniper = d.weaponFovSniper;
		g_settings.weaponFovSniperLow = d.weaponFovSniperLow;
		g_settings.weaponFovScopeOther = d.weaponFovScopeOther;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: weapons");
	}

	void ResetExtraCameras()
	{
		const Settings d;
		g_settings.parachuteFov = d.parachuteFov;
		g_settings.parachuteDistance = d.parachuteDistance;
		g_settings.vehAimFov = d.vehAimFov;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: extra cameras");
	}

	void ResetViewModes()
	{
		const Settings d;
		g_settings.pedViewMode = d.pedViewMode;
		g_settings.vehViewMode = d.vehViewMode;
		CameraControl::ResetLook();
		MarkDirty();
		Log::Line("reset: view modes");
	}

	void ResetTheme()
	{
		const Settings d;
		g_settings.themePreset = d.themePreset;
		g_settings.themeR = d.themeR;
		g_settings.themeG = d.themeG;
		g_settings.themeB = d.themeB;
		MarkDirty();
		Log::Line("reset: colours");
	}

	// Language list for the Settings row. Latin names, because the list has to be
	// readable in whichever language is active right now; the menu runs each of
	// these through the translator.
	std::vector<std::string> LanguageNames()
	{
		std::vector<std::string> names;
		for (int i = 0; i < (int)Lang::COUNT; i++)
			names.push_back(Lang::DisplayName((Lang::Id)i));
		return names;
	}

	// Reticle row: index 0 is "leave the game alone", which a plain toggle
	// cannot express - a toggle would have to pick a side the first time it runs.
	const std::vector<std::string> RETICLE_NAMES = { "Default", "Always", "Never" };

	// Small top-left style notification, the same one the game uses for pickups.
	void Notify(const char *text)
	{
		UI::_SET_NOTIFICATION_TEXT_ENTRY("STRING");
		UI::_ADD_TEXT_COMPONENT_STRING(const_cast<char *>(text));
		UI::_DRAW_NOTIFICATION(FALSE, TRUE);
	}

	bool PlayerInVehicle()
	{
		Ped ped = PLAYER::PLAYER_PED_ID();
		if (ped == 0 || !ENTITY::DOES_ENTITY_EXIST(ped))
			return false;
		return PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE) != 0;
	}

	// ------------------------------------------------------------------
	// menu
	// ------------------------------------------------------------------

	void BuildMenu()
	{
		MenuSystem &m = g_menu;

		// One page per camera. The titles are composed from two strings that are
		// already in the translation table ("ON FOOT" / "VEHICLE" and "Third
		// Person" / "First Person"), so adding a camera page needs no new
		// translation entries - it reads "YAYA - Üçüncü Şahıs" in Turkish, and so
		// on, for free.
		const std::string pedTpTitle = std::string(Lang::Tr("ON FOOT")) + " - " + Lang::Tr("Third Person");
		const std::string pedFpTitle = std::string(Lang::Tr("ON FOOT")) + " - " + Lang::Tr("First Person");
		const std::string vehTpTitle = std::string(Lang::Tr("VEHICLE")) + " - " + Lang::Tr("Third Person");
		const std::string vehFpTitle = std::string(Lang::Tr("VEHICLE")) + " - " + Lang::Tr("First Person");

		// ---------------- main -----------------------------------------
		m.AddPage(Lang::Tr("Main"));
		m.Toggle(PAGE_MAIN, Lang::Tr("Mod Enabled"), &g_settings.enabled);
		m.Label(PAGE_MAIN, []() -> std::string
		{
			if (!g_settings.enabled)
				return Lang::Tr("Mod is OFF - nothing is applied.");
			return Lang::Tr("Driving the game's own camera. Mouse look stays normal.");
		});
		m.Label(PAGE_MAIN, []() -> std::string
		{
			const bool inVeh = PlayerInVehicle();
			char buf[256];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, Lang::Tr("Now: %s / %s   (%s)"),
				inVeh ? Lang::Tr("VEHICLE") : Lang::Tr("ON FOOT"),
				inVeh ? (CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE() == 4 ? Lang::Tr("first person") : Lang::Tr("third person"))
				      : (CAM::GET_FOLLOW_PED_CAM_VIEW_MODE() == 4 ? Lang::Tr("first person") : Lang::Tr("third person")),
				Lang::Tr(CamMeta::Status()));
			return std::string(buf);
		});
		m.Separator(PAGE_MAIN);
		m.Submenu(PAGE_MAIN, pedTpTitle.c_str(), PAGE_PED_TP);
		m.Submenu(PAGE_MAIN, pedFpTitle.c_str(), PAGE_PED_FP);
		m.Submenu(PAGE_MAIN, vehTpTitle.c_str(), PAGE_VEH_TP);
		m.Submenu(PAGE_MAIN, vehFpTitle.c_str(), PAGE_VEH_FP);
		m.Submenu(PAGE_MAIN, Lang::Tr("Weapons"), PAGE_WEAPONS);
		m.Submenu(PAGE_MAIN, Lang::Tr("Extra Cameras"), PAGE_EXTRA_CAMS);
		m.Separator(PAGE_MAIN);
		m.Submenu(PAGE_MAIN, Lang::Tr("Colours"), PAGE_THEME);
		m.Submenu(PAGE_MAIN, Lang::Tr("Key Bindings"), PAGE_KEYS);
		m.Submenu(PAGE_MAIN, Lang::Tr("Settings"), PAGE_SETTINGS);
		m.Submenu(PAGE_MAIN, Lang::Tr("About"), PAGE_ABOUT);
		// The old "Open Key" row used to sit here, one step away from the top of
		// the main page. Landing on it by accident and pressing a key rebound the
		// menu-open key on the spot (Enter was a common casualty, and then Enter
		// both opened and selected). The binding now lives only on the Key
		// Bindings page, behind its own submenu, where it cannot be hit by
		// accident.

		// The view forcing and the metadata layer used to sit on their own
		// "Camera" page, between the four camera pages. It held almost nothing
		// and its rows only did something once the player changed them, so the
		// two real settings moved to the Settings page and the page is gone.

		// ---------------- camera: on foot, third person ------------------
		m.AddPage(pedTpTitle);
		m.Action(PAGE_PED_TP, Lang::Tr("Reset On-Foot Third Person"), []() { ResetOnFootThirdPerson(); });
		m.Slider(PAGE_PED_TP, Lang::Tr("Field Of View"), &g_settings.pedTpFov, 20.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_PED_TP, Lang::Tr("Follow Distance"), &g_settings.pedTpDistance, 0.0f, 4.0f, 0.025f, "");
		m.Slider(PAGE_PED_TP, Lang::Tr("Pivot Offset"), &g_settings.pedTpPivotX, -1.0f, 1.0f, 0.010f, "");
		m.Slider(PAGE_PED_TP, Lang::Tr("Tilt Limit Up"), &g_settings.pedTpTiltUp, 0.0f, 89.0f, 1.0f, " deg");
		m.Slider(PAGE_PED_TP, Lang::Tr("Tilt Limit Down"), &g_settings.pedTpTiltDown, 0.0f, 89.0f, 1.0f, " deg");
		m.Toggle(PAGE_PED_TP, Lang::Tr("Keep FOV While Aiming"), &g_settings.metaAiming);
		m.Label(PAGE_PED_TP, Lang::Tr("On = same FOV down the sights. Off = aim stays vanilla."));
		m.Separator(PAGE_PED_TP);
		m.Label(PAGE_PED_TP, Lang::Tr("While Aiming"));
		m.Slider(PAGE_PED_TP, Lang::Tr("Aiming Field Of View"), &g_settings.pedAimFov, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_PED_TP, Lang::Tr("Aiming Horizontal Origin"), &g_settings.pedAimPivotX, -1.0f, 1.0f, 0.010f, "");
		m.Slider(PAGE_PED_TP, Lang::Tr("Aiming Follow Distance"), &g_settings.pedAimDistance, 0.0f, 4.0f, 0.025f, "");
		m.Label(PAGE_PED_TP, Lang::Tr("0 = the same as third person."));

		// ---------------- camera: on foot, first person ------------------
		m.AddPage(pedFpTitle);
		m.Action(PAGE_PED_FP, Lang::Tr("Reset On-Foot First Person"), []() { ResetOnFootFirstPerson(); });
		m.Slider(PAGE_PED_FP, Lang::Tr("Field Of View"), &g_settings.pedFpFov, 20.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_PED_FP, Lang::Tr("Tilt Limit Up"), &g_settings.pedFpTiltUp, 0.0f, 89.0f, 1.0f, " deg");
		m.Slider(PAGE_PED_FP, Lang::Tr("Tilt Limit Down"), &g_settings.pedFpTiltDown, 0.0f, 89.0f, 1.0f, " deg");
		m.Separator(PAGE_PED_FP);
		m.Option(PAGE_PED_FP, Lang::Tr("Unarmed Reticle"), &g_settings.pedFpReticle, RETICLE_NAMES);
		m.Separator(PAGE_PED_FP);
		m.Slider(PAGE_PED_FP, Lang::Tr("Camera Shake"), &g_settings.shakeOnFoot, 0.0f, 300.0f, 5.0f, " %");
		m.Label(PAGE_PED_FP, Lang::Tr("0 = off. 100 is a clearly visible shake."));

		// ---------------- camera: vehicle, third person ------------------
		m.AddPage(vehTpTitle);
		m.Action(PAGE_VEH_TP, Lang::Tr("Reset Vehicle Third Person"), []() { ResetVehicleThirdPerson(); });
		m.Slider(PAGE_VEH_TP, Lang::Tr("Field Of View"), &g_settings.vehTpFov, 20.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Follow Distance"), &g_settings.vehTpDistance, 0.0f, 4.0f, 0.025f, "");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Pivot Scale"), &g_settings.vehTpPivotScale, 0.0f, 2.0f, 0.05f, "");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Follow Zoom"), &g_settings.vehTpZoom, 0.0f, 3.0f, 1.0f, "");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Tilt Limit Up"), &g_settings.vehTpTiltUp, 0.0f, 89.0f, 1.0f, " deg");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Tilt Limit Down"), &g_settings.vehTpTiltDown, 0.0f, 89.0f, 1.0f, " deg");
		m.Slider(PAGE_VEH_TP, Lang::Tr("Camera Shake"), &g_settings.shakeInVehicle, 0.0f, 300.0f, 5.0f, " %");

		// ---------------- camera: vehicle, first person ------------------
		m.AddPage(vehFpTitle);
		m.Action(PAGE_VEH_FP, Lang::Tr("Reset Vehicle First Person"), []() { ResetVehicleFirstPerson(); });
		m.Slider(PAGE_VEH_FP, Lang::Tr("Field Of View"), &g_settings.vehFpFov, 20.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_VEH_FP, Lang::Tr("View Offset Z"), &g_settings.vehFpOffsetZ, -0.5f, 0.5f, 0.010f, " m");
		m.Slider(PAGE_VEH_FP, Lang::Tr("Tilt Limit Up"), &g_settings.vehFpTiltUp, 0.0f, 89.0f, 1.0f, " deg");
		m.Slider(PAGE_VEH_FP, Lang::Tr("Tilt Limit Down"), &g_settings.vehFpTiltDown, 0.0f, 89.0f, 1.0f, " deg");
		m.Separator(PAGE_VEH_FP);
		m.Toggle(PAGE_VEH_FP, Lang::Tr("Keep First Person In Water"), &g_settings.vehFpNoSwapInWater);
		m.Toggle(PAGE_VEH_FP, Lang::Tr("Keep First Person When Wrecked"), &g_settings.vehFpNoSwapOnDestroyed);
		m.Label(PAGE_VEH_FP, Lang::Tr("The game normally forces third person for both."));

		// ---------------- theme -----------------------------------------
		m.AddPage(Lang::Tr("Colours"));
		m.Action(PAGE_THEME, Lang::Tr("RESET THIS PAGE"), []() { ResetTheme(); });
		m.Label(PAGE_THEME, Lang::Tr("Pick a preset, or use Custom and set R/G/B."));
		m.Option(PAGE_THEME, Lang::Tr("Preset"), &g_settings.themePreset, THEME_NAMES);
		m.Separator(PAGE_THEME);
		m.SliderInt(PAGE_THEME, Lang::Tr("Red"), &g_settings.themeR, 0, 255, 5);
		m.SliderInt(PAGE_THEME, Lang::Tr("Green"), &g_settings.themeG, 0, 255, 5);
		m.SliderInt(PAGE_THEME, Lang::Tr("Blue"), &g_settings.themeB, 0, 255, 5);
		m.Label(PAGE_THEME, Lang::Tr("Text switches black/white on its own to stay readable."));
		m.Separator(PAGE_THEME);
		m.Action(PAGE_THEME, Lang::Tr("Re-apply Selected Preset"), []()
		{
			ApplyThemePreset();
			MarkDirty();
		});
		m.Label(PAGE_THEME, Lang::Tr("Moving an RGB slider switches the preset to Custom."));

		// ---------------- keys ------------------------------------------
		m.AddPage(Lang::Tr("Key Bindings"));
		m.Label(PAGE_KEYS, Lang::Tr("Select a row, then press a key. ESC cancels."));
		m.Label(PAGE_KEYS, Lang::Tr("Left/Right repeat while held - just hold them."));
		m.Separator(PAGE_KEYS);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Open Menu (primary)"), &g_settings.keyPrimary);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Open Menu (extra 1)"), &g_settings.keyAlt1);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Open Menu (extra 2)"), &g_settings.keyAlt2);
		m.Separator(PAGE_KEYS);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Navigate Up"), &g_settings.keyUp);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Navigate Down"), &g_settings.keyDown);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Decrease / Left"), &g_settings.keyLeft);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Increase / Right"), &g_settings.keyRight);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Select"), &g_settings.keySelect);
		m.KeyBind(PAGE_KEYS, Lang::Tr("Back"), &g_settings.keyBack);
		m.Separator(PAGE_KEYS);
		m.Label(PAGE_KEYS, Lang::Tr("Arrows + Enter + Backspace always work as a fallback."));
		m.Action(PAGE_KEYS, Lang::Tr("Restore Default Keys"), []()
		{
			ResetKeysToDefault(Settings());
			MarkDirty();
			Log::Line("keys: restored defaults");
		});

		// ---------------- settings --------------------------------------
		m.AddPage(Lang::Tr("Settings"));
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Auto Save Config"), &g_settings.autoSave);
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Debug Overlay"), &g_settings.debugOverlay);
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Check For Updates On Launch"), &g_settings.checkUpdates);
		m.Option(PAGE_SETTINGS, Lang::Tr("Language"), &g_settings.language, LanguageNames());
		m.Label(PAGE_SETTINGS, Lang::Tr("Applies to the menu and the on-screen text."));

		// These came from the old "Camera" page - they are settings, so they live
		// on the Settings page now.
		m.Separator(PAGE_SETTINGS);
		m.Option(PAGE_SETTINGS, Lang::Tr("On-Foot View"), &g_settings.pedViewMode, VIEW_NAMES);
		m.Option(PAGE_SETTINGS, Lang::Tr("Vehicle View"), &g_settings.vehViewMode, VIEW_NAMES);
		m.Action(PAGE_SETTINGS, Lang::Tr("Reset View / POV"), []() { ResetViewModes(); });
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Metadata Layer (FOV etc.)"), &g_settings.experimentalMeta);
		m.Label(PAGE_SETTINGS, []() -> std::string
		{
			return CamMeta::Available()
				? Lang::Tr("metadata: active on this build.")
				: Lang::Tr("metadata: NOT found - FOV sliders will do nothing.");
		});

		m.Separator(PAGE_SETTINGS);
		m.Label(PAGE_SETTINGS, Lang::Tr("Share Preset Code"));
		m.Action(PAGE_SETTINGS, Lang::Tr("Export Code (Copy)"), []()
		{
			const std::string code = Config::ExportCode(g_settings);
			if (CopyTextToClipboard(code))
			{
				Log::Line("preset: exported code (%u chars)", (unsigned)code.size());
				Notify("Preset code copied to clipboard");
			}
			else
			{
				Log::Line("preset: failed to copy code to clipboard");
				Notify("Failed to access clipboard");
			}
		});
		m.Action(PAGE_SETTINGS, Lang::Tr("Import Code (Paste)"), []()
		{
			std::string code;
			if (!ReadTextFromClipboard(code) || !Config::ImportCode(code, g_settings))
			{
				Log::Line("preset: import failed (invalid or empty clipboard code)");
				Notify("Invalid preset code in clipboard");
				return;
			}
			CamMeta::RestoreAll();
			CameraControl::ResetLook();
			MarkDirty();
			Log::Line("preset: imported code from clipboard");
			Notify("Preset code imported");
		});
		m.Separator(PAGE_SETTINGS);
		m.Action(PAGE_SETTINGS, Lang::Tr("Save Config Now"), []()
		{
			SaveSettingsNow();
			Log::Line("settings: manual save");
		});
		m.Action(PAGE_SETTINGS, Lang::Tr("Reload Config From Disk"), []()
		{
			Config::Load(g_iniPath, g_settings);
			SanitizeOpenKeys();
			CamMeta::RestoreAll();
			CameraControl::ResetLook();
		});
		m.Action(PAGE_SETTINGS, Lang::Tr("Reset All To Defaults"), []()
		{
			g_settings = Settings();
			CamMeta::RestoreAll();
			CameraControl::ResetLook();
			MarkDirty();
			Log::Line("settings: reset to defaults");
		});
		m.Separator(PAGE_SETTINGS);
		m.Label(PAGE_SETTINGS, Lang::Tr("Files sit in scripts\\Cam Control\\:"));
		m.Label(PAGE_SETTINGS, Lang::Tr("CamControl.ini   (config)"));
		m.Label(PAGE_SETTINGS, Lang::Tr("CamControl.log   (log)"));

		// ---------------- dev options ------------------------------------
		// Writes the live camera's metadata object to CamControl.log. This is how
		// the field offset of an option the mod does not support yet is found:
		// dump, change one setting in the game's own camera menu, dump again and
		// compare. It only reads, so it cannot disturb anything.
		m.Separator(PAGE_SETTINGS);
		m.Label(PAGE_SETTINGS, Lang::Tr("Dev Options"));
		m.Action(PAGE_SETTINGS, Lang::Tr("Dump Camera Data To Log"), []() { CameraControl::DumpMetadata(); });
		m.Action(PAGE_SETTINGS, Lang::Tr("List Camera Hashes"), []() { CamMeta::ListHashes(); });
		m.Separator(PAGE_SETTINGS);
		// Live probe: writes one value into the camera metadata while the menu is
		// open, so a field can be identified by watching the camera react. Used to
		// add a new option whose offset is not known yet.
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Probe Camera Field"), &g_settings.probeEnabled);
		m.SliderInt(PAGE_SETTINGS, Lang::Tr("Probe Offset"), &g_settings.probeOffset, 0, 1532, 4);
		m.Slider(PAGE_SETTINGS, Lang::Tr("Probe Value"), &g_settings.probeValue, -500.0f, 500.0f, 0.25f, "");
		m.Label(PAGE_SETTINGS, Lang::Tr("Probe writes while the menu is open only."));
		m.Separator(PAGE_SETTINGS);
		// Automatic sweep: tests every offset and logs what each one does to the
		// camera, so the field table can be built without one test per restart.
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Sweep Camera Fields"), &g_settings.sweepEnabled);
		m.SliderInt(PAGE_SETTINGS, Lang::Tr("Sweep Start Offset"), &g_settings.sweepFrom, 0, 1532, 4);
		m.Toggle(PAGE_SETTINGS, Lang::Tr("Disable Idle Animation"), &g_settings.sweepDisableIdleAnims);
		m.Label(PAGE_SETTINGS, Lang::Tr("Sweep runs until switched off."));

		// ---------------- about -----------------------------------------
		m.AddPage(Lang::Tr("About"));
		m.Label(PAGE_ABOUT, MOD_NAME);
		const std::string aboutVersion = std::string(Lang::Tr("Version")) + " " MOD_VERSION;
		m.Label(PAGE_ABOUT, aboutVersion.c_str());
		m.Label(PAGE_ABOUT, Lang::Tr("made by sensin"));
		m.Label(PAGE_ABOUT, "akasensin on github");

		// ---------------- weapons ---------------------------------------
		// Added last so the pages above keep their numbers. The game picks a
		// different aiming camera per weapon class, and only the classes listed
		// here have one of their own - see camerameta.cpp for where the hashes
		// and the two FOV offsets come from.
		m.AddPage(Lang::Tr("Weapons"));
		m.Action(PAGE_WEAPONS, Lang::Tr("Reset Weapons"), []() { ResetWeapons(); });
		m.Label(PAGE_WEAPONS, Lang::Tr("POV of the camera the game aims each weapon through."));
		m.Label(PAGE_WEAPONS, Lang::Tr("0 = leave the game's value alone. Only used while aiming."));
		m.Separator(PAGE_WEAPONS);
		m.Slider(PAGE_WEAPONS, Lang::Tr("Default (all other weapons)"), &g_settings.weaponFovDefault, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Hip Fire"), &g_settings.weaponFovHip, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("RPG / Launchers"), &g_settings.weaponFovRpg, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Shotgun"), &g_settings.weaponFovShotgun, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Thrown"), &g_settings.weaponFovThrown, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Melee / Animals"), &g_settings.weaponFovMelee, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Digiscanner"), &g_settings.weaponFovDigiscan, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Jerry Can"), &g_settings.weaponFovJerry, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Sniper"), &g_settings.weaponFovSniper, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Sniper (Low Zoom)"), &g_settings.weaponFovSniperLow, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_WEAPONS, Lang::Tr("Other Scopes / Aim"), &g_settings.weaponFovScopeOther, 0.0f, 110.0f, 1.0f, " deg");

		// ---------------- extra cameras ----------------------------------
		m.AddPage(Lang::Tr("Extra Cameras"));
		m.Action(PAGE_EXTRA_CAMS, Lang::Tr("Reset Extra Cameras"), []() { ResetExtraCameras(); });
		m.Label(PAGE_EXTRA_CAMS, Lang::Tr("0 = leave the game's value alone."));
		m.Separator(PAGE_EXTRA_CAMS);
		m.Label(PAGE_EXTRA_CAMS, Lang::Tr("Parachute"));
		m.Slider(PAGE_EXTRA_CAMS, Lang::Tr("Parachute Field Of View"), &g_settings.parachuteFov, 0.0f, 110.0f, 1.0f, " deg");
		m.Slider(PAGE_EXTRA_CAMS, Lang::Tr("Parachute Follow Distance"), &g_settings.parachuteDistance, 0.0f, 4.0f, 0.025f, "");
		m.Separator(PAGE_EXTRA_CAMS);
		m.Label(PAGE_EXTRA_CAMS, Lang::Tr("Vehicle Weapon Aiming"));
		m.Slider(PAGE_EXTRA_CAMS, Lang::Tr("Drive-By FOV (Cars/Bikes/Boats/Helis)"), &g_settings.vehAimFov, 0.0f, 110.0f, 1.0f, " deg");
	}

	// ------------------------------------------------------------------
	// overlays
	// ------------------------------------------------------------------

	void DrawOverlay()
	{
		const int white[4] = { 255, 255, 255, 255 };
		const int dim[4] = { 205, 205, 205, 255 };
		const int bg[4] = { 0, 0, 0, 180 };

		if (g_settings.debugOverlay)
		{
			const bool inVeh = PlayerInVehicle();

			char l1[160], l2[160], l3[160], l4[160], l5[160];
			_snprintf_s(l1, sizeof(l1), _TRUNCATE, "%s %s", MOD_NAME, MOD_VERSION);
			_snprintf_s(l2, sizeof(l2), _TRUNCATE, "state: %s   %s",
				g_settings.enabled ? "ENABLED" : "disabled",
				CamMeta::Status());
			_snprintf_s(l3, sizeof(l3), _TRUNCATE, "%s   game cam %d",
				CameraControl::StatusText(),
				CAM::GET_RENDERING_CAM());
			_snprintf_s(l4, sizeof(l4), _TRUNCATE, "context: %s   view mode %d",
				inVeh ? "vehicle" : "on foot",
				inVeh ? CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE()
				      : CAM::GET_FOLLOW_PED_CAM_VIEW_MODE());

			// The two numbers that explain a broken picture: what the game is
			// rendering with, and what this mod last asked for. When they drift
			// apart, something else owns the camera.
			float wantFov = 0.0f;
			unsigned int wantHash = 0;
			if (CamMeta::LastWriteInfo(wantFov, wantHash))
				_snprintf_s(l5, sizeof(l5), _TRUNCATE, "fov: game %.1f   wrote %.1f (0x%X)",
					CAM::GET_GAMEPLAY_CAM_FOV(), wantFov, wantHash);
			else
				_snprintf_s(l5, sizeof(l5), _TRUNCATE, "fov: game %.1f   nothing written",
					CAM::GET_GAMEPLAY_CAM_FOV());

			const float x = 0.010f;
			const float y = 0.010f;
			const float w = 0.300f;
			const float h = 0.106f;
			Ui::FillRect(x, y, w, h, bg);
			Ui::TextLeft(l1, x + 0.008f, y + 0.004f, 0.30f, white, 1);
			Ui::TextLeft(l2, x + 0.008f, y + 0.026f, 0.26f, dim, 0);
			Ui::TextLeft(l3, x + 0.008f, y + 0.044f, 0.26f, dim, 0);
			Ui::TextLeft(l4, x + 0.008f, y + 0.062f, 0.26f, dim, 0);
			Ui::TextLeft(l5, x + 0.008f, y + 0.080f, 0.26f, dim, 0);
		}
	}

	// ------------------------------------------------------------------
	// open key
	// ------------------------------------------------------------------

	bool OpenKeyPressed()
	{
		if (g_settings.keyPrimary != 0 && IsKeyJustDown((DWORD)g_settings.keyPrimary, true))
			return true;
		if (g_settings.keyAlt1 != 0 && IsKeyJustDown((DWORD)g_settings.keyAlt1, true))
			return true;
		if (g_settings.keyAlt2 != 0 && IsKeyJustDown((DWORD)g_settings.keyAlt2, true))
			return true;
		return false;
	}
}

void ScriptSetModule(HMODULE module)
{
	g_module = module;
}

void ScriptMain()
{
	ResolvePaths();
	Log::Init(g_logPath);
	Lang::Init(g_dir);
	Log::Line("=== %s %s by %s ===", MOD_NAME, MOD_VERSION, MOD_AUTHOR);
	Log::Line("script hook game version: %d", (int)getGameVersion());
	Log::Line("config path: %ls", g_iniPath.c_str());
	Log::Line("log path:    %ls", g_logPath.c_str());

	Config::Load(g_iniPath, g_settings);

	// The dev tools write straight into game memory, so they must never come back
	// on by themselves. A crash mid-sweep would otherwise leave "sweep on" saved,
	// and the next launch would start rewriting memory before the player had a
	// chance to stop it.
	g_settings.sweepEnabled = false;
	g_settings.probeEnabled = false;

	// Before the first save, so a corrected open key is written back out.
	SanitizeOpenKeys();

	if (g_settings.autoSave)
		Config::Save(g_iniPath, g_settings);
	g_wasEnabled = g_settings.enabled;

	if (g_settings.checkUpdates)
		Updater::CheckOnLaunch(MOD_VERSION);

	// Language has to be selected before the menu is built: each row's text is
	// resolved once, when its page is created.
	Lang::Set((Lang::Id)g_settings.language);
	g_wasLanguage = g_settings.language;

	BuildMenu();

	// Let the game finish booting, then clear any script camera a previous
	// instance may have left behind (ScriptHookV's CTRL+R reloader can unload us
	// without giving the script a chance to tear its camera down).
	WAIT(1000);
	CAM::RENDER_SCRIPT_CAMS(FALSE, FALSE, 0, TRUE, TRUE);

	// Locate the camera metadata pool once. Self-disabling when not found.
	CamMeta::Init();

	Log::Line("menu ready - open key: %s (0x%02X), extras: %s / %s",
		Keys::Name(g_settings.keyPrimary).c_str(), g_settings.keyPrimary,
		Keys::Name(g_settings.keyAlt1).c_str(), Keys::Name(g_settings.keyAlt2).c_str());

	while (true)
	{
		if (OpenKeyPressed())
		{
			g_menu.ToggleOpen();
			Log::Line("menu: %s", g_menu.IsOpen() ? "opened" : "closed");
		}

		// The theme preset option writes its RGB the moment it changes, and moving
		// an RGB slider flips the preset over to Custom.
		const int presetBefore = g_settings.themePreset;
		const int rBefore = g_settings.themeR, gBefore = g_settings.themeG, bBefore = g_settings.themeB;
		g_menu.Update(g_settings);
		if (g_settings.themePreset != presetBefore)
			ApplyThemePreset();
		else if (g_settings.themeR != rBefore || g_settings.themeG != gBefore || g_settings.themeB != bBefore)
			g_settings.themePreset = THEME_CUSTOM;

		if (g_menu.ConsumeChanged())
			MarkDirty();

		// The probe and the sweep both write into the same object, so they cannot
		// run together: the probe would hold one offset at a fixed value and hide
		// it from the sweep (and vice versa). Whichever was turned on last wins.
		if (g_settings.sweepEnabled && g_settings.probeEnabled)
		{
			g_settings.probeEnabled = false;
			MarkDirty();

			static bool logged = false;
			if (!logged)
			{
				logged = true;
				Log::Line("sweep: probe turned off - the two cannot run at the same time");
			}
		}

		// Dev tools report in game, not only in the log: the sweep runs for minutes
		// and the player has nothing better to watch than the result.
		const char *devMsg = CamMeta::TakeMessage();
		if (devMsg)
			Notify(devMsg);

		// Swapping language rebuilds the pages in the new one, and re-opens the
		// page the player was on so the change does not look like a crash.
		if (g_settings.language != g_wasLanguage)
		{
			g_wasLanguage = g_settings.language;
			Lang::Set((Lang::Id)g_settings.language);
			Log::Line("lang: switched to %s", Lang::CurrentCode().c_str());

			const bool wasOpen = g_menu.IsOpen();
			g_menu.Clear();
			BuildMenu();
			if (wasOpen)
				g_menu.Open(PAGE_SETTINGS);
		}

		CameraControl::Tick(g_settings, g_menu.IsOpen());

		// The mod gives up a forced view mode as soon as the player changes it
		// with V, so that is a settings change like any other and has to be
		// saved - otherwise the next launch would start forcing again.
		if (CameraControl::ConsumeForceReleased())
			MarkDirty();

		if (g_settings.enabled != g_wasEnabled)
		{
			g_wasEnabled = g_settings.enabled;
			Log::Line("camera: %s", g_settings.enabled ? "enabled" : "disabled");
			Notify(g_settings.enabled ? "Cam Control: enabled" : "Cam Control: disabled");
			if (!g_settings.enabled)
				CameraControl::Shutdown();
		}

		// Everything the menu drew last frame goes through this, so the overlay
		// only ever shows the current frame's text.
		DxText::BeginFrame();

		DrawOverlay();
		g_menu.Draw(g_settings);

		// save shortly after the last change, and right away when the menu closes
		const bool menuJustClosed = (g_wasOpen && !g_menu.IsOpen());
		g_wasOpen = g_menu.IsOpen();

		if (g_dirty && g_settings.autoSave)
		{
			const DWORD now = GetTickCount();
			if (menuJustClosed || (now - g_dirtySince) >= SAVE_DEBOUNCE_MS)
			{
				SaveSettingsNow();
				Log::Line("settings: auto saved");
			}
		}

		WAIT(0);
	}
}
