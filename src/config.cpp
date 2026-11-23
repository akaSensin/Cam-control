/*
	Cam Control - made by sensin
*/

#include "config.h"
#include "keys.h"
#include "lang.h"
#include "log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	const int kConfigVersion = 3;

	// ---- tiny helpers ------------------------------------------------------

	void Append(std::string &out, const char *fmt, ...)
	{
		char buf[512];
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
		va_end(args);
		out += buf;
	}

	std::string Trim(const std::string &s)
	{
		size_t a = 0;
		size_t b = s.size();
		while (a < b && (unsigned char)s[a] <= ' ')
			a++;
		while (b > a && (unsigned char)s[b - 1] <= ' ')
			b--;
		return s.substr(a, b - a);
	}

	std::string Lower(const std::string &s)
	{
		std::string r = s;
		for (size_t i = 0; i < r.size(); i++)
			r[i] = (char)tolower((unsigned char)r[i]);
		return r;
	}

	bool ParseBool(const std::string &v, bool &out)
	{
		std::string l = Lower(Trim(v));
		if (l == "1" || l == "true" || l == "on" || l == "yes") { out = true; return true; }
		if (l == "0" || l == "false" || l == "off" || l == "no" || l.empty()) { out = false; return true; }
		return false;
	}

	bool ParseInt(const std::string &v, int &out)
	{
		std::string t = Trim(v);
		if (t.empty())
			return false;
		char *end = nullptr;
		long val = strtol(t.c_str(), &end, 0);
		if (end == t.c_str())
			return false;
		out = (int)val;
		return true;
	}

	bool ParseFloat(const std::string &v, float &out)
	{
		std::string t = Trim(v);
		if (t.empty())
			return false;
		char *end = nullptr;
		float val = strtof(t.c_str(), &end);
		if (end == t.c_str())
			return false;
		out = val;
		return true;
	}

	bool ReadWholeFile(const std::wstring &path, std::string &out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;

		out.clear();
		char buf[4096];
		DWORD read = 0;
		while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0)
			out.append(buf, read);
		CloseHandle(h);

		// strip UTF-8 BOM
		if (out.size() >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
			out.erase(0, 3);
		return true;
	}

	bool WriteWholeFile(const std::wstring &path, const std::string &data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;

		const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
		DWORD written = 0;
		WriteFile(h, bom, 3, &written, nullptr);

		size_t offset = 0;
		while (offset < data.size())
		{
			DWORD chunk = (DWORD)((data.size() - offset) > 65536 ? 65536 : (data.size() - offset));
			if (!WriteFile(h, data.data() + offset, chunk, &written, nullptr) || written == 0)
				break;
			offset += written;
		}
		CloseHandle(h);
		return offset == data.size();
	}

	int ClampInt(int v, int lo, int hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	// Keys that belonged to the removed scripted camera / gameplay-offset layer.
	// They are ignored silently so an old ini does not fill the log with noise.
	bool IsLegacyKey(const std::string &k)
	{
		static const char *legacy[] = {
			"backend", "scriptcam", "affectsaiming", "smooth", "autoalign", "mouselook",
			"invertpitch", "invertyaw", "showwatermark",
			"gpyawoffset", "gppitchoffset", "gpyawlimit", "gppitchlimit", "gpdistance",
			"pedfov", "peddepth", "pedheight", "pedside", "pedpitch", "pedyaw", "pedlooksens",
			"fppfov", "fppheight", "fppforward",
			"vehfov", "vehdepth", "vehheight", "vehside", "vehpitch", "vehyaw",
			"vehlooksens", "vehzoom"
		};

		for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++)
		{
			if (k == legacy[i])
				return true;
		}
		return false;
	}

	Settings Apply(const std::string &key, const std::string &value, const Settings &cur)
	{
		Settings s = cur;
		const std::string k = Lower(key);

		if (k == "enabled")                 ParseBool(value, s.enabled);
		else if (k == "experimentalmeta")   ParseBool(value, s.experimentalMeta);
		else if (k == "metaaiming")         ParseBool(value, s.metaAiming);
		else if (k == "autosave")           ParseBool(value, s.autoSave);
		else if (k == "debugoverlay")       ParseBool(value, s.debugOverlay);
		else if (k == "language")           ParseInt(value, s.language);
		else if (k == "themepreset")        ParseInt(value, s.themePreset);
		else if (k == "themer")             ParseInt(value, s.themeR);
		else if (k == "themeg")             ParseInt(value, s.themeG);
		else if (k == "themeb")             ParseInt(value, s.themeB);
		else if (k == "pedtpfov")           ParseFloat(value, s.pedTpFov);
		else if (k == "pedtpdistance")      ParseFloat(value, s.pedTpDistance);
		else if (k == "pedtppivotx")        ParseFloat(value, s.pedTpPivotX);
		else if (k == "pedtppitchoverride") ParseBool(value, s.pedTpPitchOverride);
		else if (k == "pedtpdefaultpitch")  ParseFloat(value, s.pedTpDefaultPitch);
		else if (k == "pedtppitchlimit")    { ParseFloat(value, s.pedTpTiltUp); s.pedTpTiltDown = s.pedTpTiltUp; }
		else if (k == "pedfpfov")           ParseFloat(value, s.pedFpFov);
		else if (k == "pedfpoffsetx")       ParseFloat(value, s.pedFpOffsetX);
		else if (k == "pedfppitchlimit")    { ParseFloat(value, s.pedFpTiltUp); s.pedFpTiltDown = s.pedFpTiltUp; }
		else if (k == "vehtpfov")           ParseFloat(value, s.vehTpFov);
		else if (k == "vehtpdistance")      ParseFloat(value, s.vehTpDistance);
		else if (k == "vehtppivotx")        ParseFloat(value, s.vehTpPivotX);
		else if (k == "vehtppivotscale")    ParseFloat(value, s.vehTpPivotScale);
		else if (k == "vehtpzoom")          ParseFloat(value, s.vehTpZoom);
		else if (k == "vehtppitchlimit")    { ParseFloat(value, s.vehTpTiltUp); s.vehTpTiltDown = s.vehTpTiltUp; }
		else if (k == "vehfpfov")           ParseFloat(value, s.vehFpFov);
		else if (k == "vehfpoffsetx")       ParseFloat(value, s.vehFpOffsetX);
		else if (k == "vehfpoffsetz")       ParseFloat(value, s.vehFpOffsetZ);
		else if (k == "vehfppitchlimit")    { ParseFloat(value, s.vehFpTiltUp); s.vehFpTiltDown = s.vehFpTiltUp; }
		// Split tilt limits, one value per direction.
		else if (k == "pedtptiltup")        ParseFloat(value, s.pedTpTiltUp);
		else if (k == "pedtptiltdown")      ParseFloat(value, s.pedTpTiltDown);
		else if (k == "pedfptiltup")        ParseFloat(value, s.pedFpTiltUp);
		else if (k == "pedfptiltdown")      ParseFloat(value, s.pedFpTiltDown);
		else if (k == "vehtptiltup")        ParseFloat(value, s.vehTpTiltUp);
		else if (k == "vehtptiltdown")      ParseFloat(value, s.vehTpTiltDown);
		else if (k == "vehfptiltup")        ParseFloat(value, s.vehFpTiltUp);
		else if (k == "vehfptiltdown")      ParseFloat(value, s.vehFpTiltDown);
		// Aiming camera - 0 means "same as third person", the old behaviour.
		else if (k == "pedaimfov")          ParseFloat(value, s.pedAimFov);
		else if (k == "pedaimdistance")     ParseFloat(value, s.pedAimDistance);
		else if (k == "pedaimpivotx")       ParseFloat(value, s.pedAimPivotX);
		// First person extras.
		else if (k == "pedfpreticle")       ParseInt(value, s.pedFpReticle);
		else if (k == "pedfpaimfov")        { /* removed: non-scoped FP ADS FOV is overridden per weapon by the game */ }
		// Camera shake, percent of the game's own.
		else if (k == "shakeonfoot")        ParseFloat(value, s.shakeOnFoot);
		else if (k == "shakeinvehicle")     ParseFloat(value, s.shakeInVehicle);
		// Stop the game forcing the camera out of first person.
		else if (k == "vehfpnoswapinwater")     ParseBool(value, s.vehFpNoSwapInWater);
		else if (k == "vehfpnoswapondestroyed") ParseBool(value, s.vehFpNoSwapOnDestroyed);
		else if (k == "pedviewmode")        ParseInt(value, s.pedViewMode);
		else if (k == "vehviewmode")        ParseInt(value, s.vehViewMode);
		else if (k == "probeenabled")       ParseBool(value, s.probeEnabled);
		else if (k == "probeoffset")        ParseInt(value, s.probeOffset);
		else if (k == "probevalue")         ParseFloat(value, s.probeValue);
		else if (k == "sweepenabled")       ParseBool(value, s.sweepEnabled);
		else if (k == "sweepfrom")          ParseInt(value, s.sweepFrom);
		else if (k == "sweepdisableidleans") ParseBool(value, s.sweepDisableIdleAnims);
		else if (k == "weaponfovdefault")    ParseFloat(value, s.weaponFovDefault);
		else if (k == "weaponfovhip")        ParseFloat(value, s.weaponFovHip);
		else if (k == "weaponfovrpg")        ParseFloat(value, s.weaponFovRpg);
		else if (k == "weaponfovshotgun")    ParseFloat(value, s.weaponFovShotgun);
		else if (k == "weaponfovthrown")     ParseFloat(value, s.weaponFovThrown);
		else if (k == "weaponfovmelee")      ParseFloat(value, s.weaponFovMelee);
		else if (k == "weaponfovdigiscan")   ParseFloat(value, s.weaponFovDigiscan);
		else if (k == "weaponfovjerry")      ParseFloat(value, s.weaponFovJerry);
		else if (k == "weaponfovsniper")     ParseFloat(value, s.weaponFovSniper);
		else if (k == "weaponfovsniperlow")  ParseFloat(value, s.weaponFovSniperLow);
		else if (k == "weaponfovscopeother") ParseFloat(value, s.weaponFovScopeOther);
		else if (k == "parachutefov")        ParseFloat(value, s.parachuteFov);
		else if (k == "parachutedistance")   ParseFloat(value, s.parachuteDistance);
		else if (k == "vehaimfov")           ParseFloat(value, s.vehAimFov);
		else if (IsLegacyKey(k))            { /* removed with the scripted camera */ }
		else if (k == "keyprimary")         Keys::FromName(value, s.keyPrimary);
		else if (k == "keyalt1")            Keys::FromName(value, s.keyAlt1);
		else if (k == "keyalt2")            Keys::FromName(value, s.keyAlt2);
		else if (k == "keyup")              Keys::FromName(value, s.keyUp);
		else if (k == "keydown")            Keys::FromName(value, s.keyDown);
		else if (k == "keyleft")            Keys::FromName(value, s.keyLeft);
		else if (k == "keyright")           Keys::FromName(value, s.keyRight);
		else if (k == "keyselect")          Keys::FromName(value, s.keySelect);
		else if (k == "keyback")            Keys::FromName(value, s.keyBack);
		else if (k == "configversion")      { /* handled by the loader */ }
		else if (!k.empty())
			Log::Line("config: unknown key '%s' ignored", key.c_str());

		return s;
	}
}

namespace Config
{
	bool Load(const std::wstring &path, Settings &s)
	{
		std::string data;
		if (!ReadWholeFile(path, data))
		{
			Log::Line("config: '%ls' not found, using defaults", path.c_str());
			return false;
		}

		Settings parsed;
		int applied = 0;
		int version = 0;

		size_t pos = 0;
		while (pos <= data.size())
		{
			size_t eol = data.find('\n', pos);
			std::string line = (eol == std::string::npos) ? data.substr(pos) : data.substr(pos, eol - pos);
			pos = (eol == std::string::npos) ? data.size() + 1 : eol + 1;

			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[')
				continue;

			size_t eq = line.find('=');
			if (eq == std::string::npos)
				continue;

			std::string key = Trim(line.substr(0, eq));
			std::string value = Trim(line.substr(eq + 1));
			if (Lower(key) == "configversion")
				ParseInt(value, version);

			parsed = Apply(key, value, parsed);
			applied++;
		}

		// --- migration ---------------------------------------------------
		if (version < kConfigVersion)
		{
			// The scripted camera and the gameplay-offset layer were removed in
			// this version; Apply() ignores their keys. The values that replaced
			// them already carry sensible defaults, so nothing is rewritten here.
			Log::Line("config: upgrading %d -> %d (scripted camera removed)", version, kConfigVersion);
		}

		// --- sanity ------------------------------------------------------
		parsed.themeR = ClampInt(parsed.themeR, 0, 255);
		parsed.themeG = ClampInt(parsed.themeG, 0, 255);
		parsed.themeB = ClampInt(parsed.themeB, 0, 255);
		if (parsed.themePreset < 0 || parsed.themePreset >= THEME_COUNT)
			parsed.themePreset = THEME_MONOCHROME;
		if (parsed.pedViewMode < 0 || parsed.pedViewMode > 2)
			parsed.pedViewMode = VIEWMODE_DONT_TOUCH;
		if (parsed.vehViewMode < 0 || parsed.vehViewMode > 2)
			parsed.vehViewMode = VIEWMODE_DONT_TOUCH;
		parsed.pedFpReticle = ClampInt(parsed.pedFpReticle, 0, 2);
		if (parsed.language < 0 || parsed.language >= Lang::COUNT)
			parsed.language = 0;

		s = parsed;
		Log::Line("config: loaded %d entries from '%ls'", applied, path.c_str());
		return true;
	}

	bool Save(const std::wstring &path, const Settings &s)
	{
		std::string out;
		out += "; Cam Control - configuration file (made by sensin)\r\n";
		out += "; Values are read back on every game start. Edit in game or by hand.\r\n";
		out += "; Keys accept key names (-, F8, K, UP, ENTER, NUM 5, UNBOUND) or VK numbers.\r\n";
		out += "\r\n[General]\r\n";
		Append(out, "ConfigVersion=%d\r\n", kConfigVersion);
		Append(out, "Enabled=%d\r\n",          s.enabled ? 1 : 0);
		Append(out, "; Write the game's camera metadata for fov / distance / pivot.\r\n");
		Append(out, "; Those values have no native, so this is the only build specific\r\n");
		Append(out, "; part. 0 = natives only (safe), 1 = allow the metadata writes.\r\n");
		Append(out, "ExperimentalMeta=%d\r\n", s.experimentalMeta ? 1 : 0);
		Append(out, "; Also apply the on-foot FOV to the aiming camera. 0 = aim stays vanilla.\r\n");
		Append(out, "MetaAiming=%d\r\n",       s.metaAiming ? 1 : 0);
		Append(out, "AutoSave=%d\r\n",         s.autoSave ? 1 : 0);
		Append(out, "DebugOverlay=%d\r\n",     s.debugOverlay ? 1 : 0);
		Append(out, "; Language: 0 English, 1 Turkish, 2 Spanish, 3 Russian, 4 Arabic,\r\n");
		Append(out, ";           5 Chinese, 6 Japanese, 7 German, 8 Portuguese,\r\n");
		Append(out, ";           9 Indonesian.\r\n");
		Append(out, ";           Arabic/Chinese/Japanese/German/Portuguese plus Spanish are\r\n");
		Append(out, ";           drawn by the mod's own DirectWrite overlay - see README.\r\n");
		Append(out, "Language=%d\r\n",         s.language);

		out += "\r\n[Theme]\r\n";
		Append(out, "; ThemePreset: 0 = Custom (use ThemeR/G/B), 1 = Monochrome, 2 = Amber,\r\n");
		Append(out, ";              3 = Emerald, 4 = Cyan, 5 = Crimson, 6 = Violet, 7 = Blue\r\n");
		Append(out, "ThemePreset=%d\r\n",      s.themePreset);
		Append(out, "ThemeR=%d\r\n",           s.themeR);
		Append(out, "ThemeG=%d\r\n",           s.themeG);
		Append(out, "ThemeB=%d\r\n",           s.themeB);

		out += "\r\n[ViewMode]\r\n";
		Append(out, "; 0 = do not touch the game setting, 1 = third person, 2 = first person\r\n");
		Append(out, "PedViewMode=%d\r\n",      s.pedViewMode);
		Append(out, "VehViewMode=%d\r\n",      s.vehViewMode);

		out += "\r\n[Dev]\r\n";
		Append(out, "; Live metadata probe - finds the field offset of a camera option the\r\n");
		Append(out, "; mod does not support yet. Off by default. See README, section \"DEVELOPER\r\n");
		Append(out, "; OPTIONS\". Writes only while the menu is open.\r\n");
		Append(out, "ProbeEnabled=%d\r\n",     s.probeEnabled ? 1 : 0);
		Append(out, "ProbeOffset=%d\r\n",      s.probeOffset);
		Append(out, "ProbeValue=%.6g\r\n",     s.probeValue);
		Append(out, "SweepEnabled=%d\r\n",     s.sweepEnabled ? 1 : 0);
		Append(out, "SweepFrom=%d\r\n",        s.sweepFrom);
		Append(out, "SweepDisableIdleAnims=%d\r\n", s.sweepDisableIdleAnims ? 1 : 0);

		out += "\r\n[OnFootThirdPerson]\r\n";
		Append(out, "PedTpFov=%.6g\r\n",       s.pedTpFov);
		Append(out, "PedTpDistance=%.6g\r\n",  s.pedTpDistance);
		Append(out, "PedTpPivotX=%.6g\r\n",    s.pedTpPivotX);
		Append(out, "PedTpPitchOverride=%d\r\n", s.pedTpPitchOverride ? 1 : 0);
		Append(out, "PedTpDefaultPitch=%.6g\r\n", s.pedTpDefaultPitch);
		Append(out, "; The aiming camera the game switches to down the sights.\r\n");
		Append(out, "; 0 = same as third person, which is what the mod did before these\r\n");
		Append(out, "; existed, so leaving them at 0 keeps the old behaviour exactly.\r\n");
		Append(out, "PedAimFov=%.6g\r\n",      s.pedAimFov);
		Append(out, "PedAimDistance=%.6g\r\n", s.pedAimDistance);
		Append(out, "PedAimPivotX=%.6g\r\n",   s.pedAimPivotX);
		Append(out, "; Camera shake as a percentage of the game's own. 0 = leave alone,\r\n");
		Append(out, "; 100 = the game's own amount.\r\n");
		Append(out, "ShakeOnFoot=%.6g\r\n",    s.shakeOnFoot);
		Append(out, "; Tilt limits are a native, one value per direction.\r\n");
		Append(out, "; 0 = leave that direction to the game. Setting only one side works.\r\n");
		Append(out, "PedTpTiltUp=%.6g\r\n",    s.pedTpTiltUp);
		Append(out, "PedTpTiltDown=%.6g\r\n",  s.pedTpTiltDown);

		out += "\r\n[OnFootFirstPerson]\r\n";
		Append(out, "PedFpFov=%.6g\r\n",       s.pedFpFov);
		Append(out, "PedFpOffsetX=%.6g\r\n",   s.pedFpOffsetX);
		Append(out, "; Unarmed reticle: 0 = do not touch, 1 = always show, 2 = never show.\r\n");
		Append(out, "PedFpReticle=%d\r\n",     s.pedFpReticle);
		Append(out, "PedFpTiltUp=%.6g\r\n",    s.pedFpTiltUp);
		Append(out, "PedFpTiltDown=%.6g\r\n",  s.pedFpTiltDown);

		out += "\r\n[VehicleThirdPerson]\r\n";
		Append(out, "VehTpFov=%.6g\r\n",       s.vehTpFov);
		Append(out, "VehTpDistance=%.6g\r\n",  s.vehTpDistance);
		Append(out, "VehTpPivotX=%.6g\r\n",    s.vehTpPivotX);
		Append(out, "VehTpPivotScale=%.6g\r\n", s.vehTpPivotScale);
		Append(out, "; Follow zoom is a native. 0 = leave the game alone, 1..3 = Near / Med / Far.\r\n");
		Append(out, "VehTpZoom=%.6g\r\n",      s.vehTpZoom);
		Append(out, "ShakeInVehicle=%.6g\r\n", s.shakeInVehicle);
		Append(out, "VehTpTiltUp=%.6g\r\n",    s.vehTpTiltUp);
		Append(out, "VehTpTiltDown=%.6g\r\n",  s.vehTpTiltDown);

		out += "\r\n[VehicleFirstPerson]\r\n";
		Append(out, "VehFpFov=%.6g\r\n",       s.vehFpFov);
		Append(out, "VehFpOffsetX=%.6g\r\n",   s.vehFpOffsetX);
		Append(out, "VehFpOffsetZ=%.6g\r\n",   s.vehFpOffsetZ);
		Append(out, "; The game forces the camera out of first person when the vehicle enters\r\n");
		Append(out, "; deep water, and again when it is wrecked. These undo that for the\r\n");
		Append(out, "; duration of the hazard. A deliberate view change still works.\r\n");
		Append(out, "VehFpNoSwapInWater=%d\r\n",     s.vehFpNoSwapInWater ? 1 : 0);
		Append(out, "VehFpNoSwapOnDestroyed=%d\r\n", s.vehFpNoSwapOnDestroyed ? 1 : 0);
		Append(out, "VehFpTiltUp=%.6g\r\n",    s.vehFpTiltUp);
		Append(out, "VehFpTiltDown=%.6g\r\n",  s.vehFpTiltDown);

		out += "\r\n[Weapons]\r\n";
		Append(out, "; FOV of the aiming camera the game switches to per weapon class.\r\n");
		Append(out, "; 0 = leave the game's own value alone. Written only while MetaAiming=1.\r\n");
		Append(out, "; Weapons with no camera of their own (pistols, SMG, rifles) all use\r\n");
		Append(out, "; WeaponFovDefault - there is a single camera behind them.\r\n");
		Append(out, "WeaponFovDefault=%.6g\r\n",   s.weaponFovDefault);
		Append(out, "WeaponFovHip=%.6g\r\n",       s.weaponFovHip);
		Append(out, "WeaponFovRpg=%.6g\r\n",       s.weaponFovRpg);
		Append(out, "WeaponFovShotgun=%.6g\r\n",   s.weaponFovShotgun);
		Append(out, "WeaponFovThrown=%.6g\r\n",    s.weaponFovThrown);
		Append(out, "WeaponFovMelee=%.6g\r\n",     s.weaponFovMelee);
		Append(out, "WeaponFovDigiscan=%.6g\r\n",  s.weaponFovDigiscan);
		Append(out, "WeaponFovJerry=%.6g\r\n",     s.weaponFovJerry);
		Append(out, "WeaponFovSniper=%.6g\r\n",    s.weaponFovSniper);
		Append(out, "WeaponFovSniperLow=%.6g\r\n", s.weaponFovSniperLow);
		Append(out, "WeaponFovScopeOther=%.6g\r\n", s.weaponFovScopeOther);

		out += "\r\n[ExtraCameras]\r\n";
		Append(out, "ParachuteFov=%.6g\r\n",       s.parachuteFov);
		Append(out, "ParachuteDistance=%.6g\r\n",  s.parachuteDistance);
		Append(out, "VehAimFov=%.6g\r\n",          s.vehAimFov);

		out += "\r\n[Keys]\r\n";
		Append(out, "; Key names: -, F1..F24, A..Z, 1..9, KEY_0, NUM 0..9, UP/DOWN/LEFT/RIGHT,\r\n");
		Append(out, ";            ENTER, BACKSPACE, SPACE, TAB, INSERT, DELETE, UNBOUND (or VK code).\r\n");
		Append(out, "KeyPrimary=%s\r\n",       Keys::Name(s.keyPrimary).c_str());
		Append(out, "KeyAlt1=%s\r\n",          Keys::Name(s.keyAlt1).c_str());
		Append(out, "KeyAlt2=%s\r\n",          Keys::Name(s.keyAlt2).c_str());
		Append(out, "KeyUp=%s\r\n",            Keys::Name(s.keyUp).c_str());
		Append(out, "KeyDown=%s\r\n",          Keys::Name(s.keyDown).c_str());
		Append(out, "KeyLeft=%s\r\n",          Keys::Name(s.keyLeft).c_str());
		Append(out, "KeyRight=%s\r\n",         Keys::Name(s.keyRight).c_str());
		Append(out, "KeySelect=%s\r\n",        Keys::Name(s.keySelect).c_str());
		Append(out, "KeyBack=%s\r\n",          Keys::Name(s.keyBack).c_str());

		if (!WriteWholeFile(path, out))
		{
			Log::Line("config: FAILED to write '%ls' (error %lu)", path.c_str(), GetLastError());
			return false;
		}

		Log::Line("config: saved to '%ls'", path.c_str());
		return true;
	}

	namespace
	{
		const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

		int B64Val(char c)
		{
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '-' || c == '+') return 62;
			if (c == '_' || c == '/') return 63;
			return -1;
		}

		std::string Base64Encode(const unsigned char *data, size_t len)
		{
			std::string out;
			out.reserve(((len + 2) / 3) * 4);
			for (size_t i = 0; i < len; i += 3)
			{
				const unsigned int b0 = data[i];
				const unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0;
				const unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0;
				const unsigned int v = (b0 << 16) | (b1 << 8) | b2;
				out.push_back(kB64[(v >> 18) & 0x3F]);
				out.push_back(kB64[(v >> 12) & 0x3F]);
				if (i + 1 < len) out.push_back(kB64[(v >> 6) & 0x3F]);
				if (i + 2 < len) out.push_back(kB64[v & 0x3F]);
			}
			return out;
		}

		bool Base64Decode(const std::string &in, std::vector<unsigned char> &out)
		{
			out.clear();
			unsigned int acc = 0;
			int bits = 0;
			for (size_t i = 0; i < in.size(); i++)
			{
				if (in[i] == '=')
					break;
				const int v = B64Val(in[i]);
				if (v < 0)
					return false;
				acc = (acc << 6) | (unsigned int)v;
				bits += 6;
				if (bits >= 8)
				{
					bits -= 8;
					out.push_back((unsigned char)((acc >> bits) & 0xFFu));
				}
			}
			return true;
		}

		unsigned int Crc32(const unsigned char *data, size_t len)
		{
			unsigned int crc = 0xFFFFFFFFu;
			for (size_t i = 0; i < len; i++)
			{
				crc ^= data[i];
				for (int j = 0; j < 8; j++)
					crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
			}
			return crc ^ 0xFFFFFFFFu;
		}

		void WriteFloatLE(std::vector<unsigned char> &buf, float v)
		{
			unsigned char b[4];
			memcpy(b, &v, 4);
			buf.insert(buf.end(), b, b + 4);
		}

		float ReadFloatLE(const unsigned char *p)
		{
			float v = 0.0f;
			memcpy(&v, p, 4);
			return v;
		}

		float ClampF(float v, float lo, float hi, float fallback)
		{
			if (!(v == v)) // NaN
				return fallback;
			return v < lo ? lo : (v > hi ? hi : v);
		}
	}

	std::string ExportCode(const Settings &s)
	{
		std::vector<unsigned char> buf;
		buf.reserve(8 + 39 * 4);

		unsigned char flags = 0;
		if (s.enabled)                flags |= 0x01;
		if (s.experimentalMeta)       flags |= 0x02;
		if (s.metaAiming)             flags |= 0x04;
		if (s.vehFpNoSwapInWater)     flags |= 0x08;
		if (s.vehFpNoSwapOnDestroyed) flags |= 0x10;

		buf.push_back(flags);
		buf.push_back((unsigned char)(s.pedViewMode & 0xFF));
		buf.push_back((unsigned char)(s.vehViewMode & 0xFF));
		buf.push_back((unsigned char)(s.pedFpReticle & 0xFF));
		buf.push_back((unsigned char)(s.themePreset & 0xFF));
		buf.push_back((unsigned char)(s.themeR & 0xFF));
		buf.push_back((unsigned char)(s.themeG & 0xFF));
		buf.push_back((unsigned char)(s.themeB & 0xFF));

		const float floats[] =
		{
			s.pedTpFov, s.pedTpDistance, s.pedTpPivotX,
			s.pedAimFov, s.pedAimDistance, s.pedAimPivotX,
			s.pedTpTiltUp, s.pedTpTiltDown,
			s.pedFpTiltUp, s.pedFpTiltDown,
			s.vehTpTiltUp, s.vehTpTiltDown,
			s.vehFpTiltUp, s.vehFpTiltDown,
			s.pedFpFov, 0.0f, // legacy pedFpAimFov slot kept for CC1/CC2 code layout compatibility
			s.vehTpFov, s.vehTpDistance, s.vehTpPivotScale, s.vehTpZoom,
			s.vehFpFov, s.vehFpOffsetZ,
			s.shakeOnFoot, s.shakeInVehicle,
			s.weaponFovDefault, s.weaponFovHip, s.weaponFovRpg, s.weaponFovShotgun,
			s.weaponFovThrown, s.weaponFovMelee, s.weaponFovDigiscan, s.weaponFovJerry,
			s.weaponFovSniper, s.weaponFovSniperLow,
			s.weaponFovScopeOther, s.parachuteFov, s.parachuteDistance,
			s.vehAimFov, s.vehFpOffsetX
		};
		for (size_t i = 0; i < sizeof(floats) / sizeof(floats[0]); i++)
			WriteFloatLE(buf, floats[i]);

		const unsigned int crc = Crc32(&buf[0], buf.size());
		char tail[16];
		_snprintf_s(tail, sizeof(tail), _TRUNCATE, ":%08X", crc);
		return std::string("CC2:") + Base64Encode(&buf[0], buf.size()) + tail;
	}

	bool ImportCode(const std::string &rawCode, Settings &s)
	{
		std::string code = Trim(rawCode);
		const bool isV1 = (code.size() >= 14 && code.compare(0, 4, "CC1:") == 0);
		const bool isV2 = (code.size() >= 14 && code.compare(0, 4, "CC2:") == 0);
		if (!isV1 && !isV2)
			return false;

		const size_t lastColon = code.rfind(':');
		if (lastColon <= 4 || lastColon + 1 >= code.size())
			return false;

		const std::string payloadB64 = code.substr(4, lastColon - 4);
		const std::string crcHex = code.substr(lastColon + 1);
		if (crcHex.size() != 8)
			return false;

		char *end = nullptr;
		const unsigned long wantCrc = strtoul(crcHex.c_str(), &end, 16);
		if (end == crcHex.c_str() || *end != '\0')
			return false;

		const size_t expectedLen = isV2 ? (8 + 39 * 4) : (8 + 34 * 4);
		std::vector<unsigned char> buf;
		if (!Base64Decode(payloadB64, buf) || buf.size() != expectedLen)
			return false;

		if (Crc32(&buf[0], buf.size()) != (unsigned int)wantCrc)
			return false;

		const unsigned char flags = buf[0];
		s.enabled                = (flags & 0x01) != 0;
		s.experimentalMeta       = (flags & 0x02) != 0;
		s.metaAiming             = (flags & 0x04) != 0;
		s.vehFpNoSwapInWater     = (flags & 0x08) != 0;
		s.vehFpNoSwapOnDestroyed = (flags & 0x10) != 0;

		s.pedViewMode  = (buf[1] <= 2) ? (int)buf[1] : VIEWMODE_DONT_TOUCH;
		s.vehViewMode  = (buf[2] <= 2) ? (int)buf[2] : VIEWMODE_DONT_TOUCH;
		s.pedFpReticle = (buf[3] <= 2) ? (int)buf[3] : 0;
		s.themePreset  = (buf[4] < THEME_COUNT) ? (int)buf[4] : THEME_MONOCHROME;
		s.themeR       = (int)buf[5];
		s.themeG       = (int)buf[6];
		s.themeB       = (int)buf[7];

		const Settings def;
		const unsigned char *p = &buf[8];
		s.pedTpFov           = ClampF(ReadFloatLE(p + 0  * 4), 20.0f, 110.0f, def.pedTpFov);
		s.pedTpDistance      = ClampF(ReadFloatLE(p + 1  * 4), 0.0f,  4.0f,   def.pedTpDistance);
		s.pedTpPivotX        = ClampF(ReadFloatLE(p + 2  * 4), -1.0f, 1.0f,   def.pedTpPivotX);
		s.pedAimFov          = ClampF(ReadFloatLE(p + 3  * 4), 0.0f,  110.0f, def.pedAimFov);
		s.pedAimDistance     = ClampF(ReadFloatLE(p + 4  * 4), 0.0f,  4.0f,   def.pedAimDistance);
		s.pedAimPivotX       = ClampF(ReadFloatLE(p + 5  * 4), -1.0f, 1.0f,   def.pedAimPivotX);
		s.pedTpTiltUp        = ClampF(ReadFloatLE(p + 6  * 4), 0.0f,  89.0f,  def.pedTpTiltUp);
		s.pedTpTiltDown      = ClampF(ReadFloatLE(p + 7  * 4), 0.0f,  89.0f,  def.pedTpTiltDown);
		s.pedFpTiltUp        = ClampF(ReadFloatLE(p + 8  * 4), 0.0f,  89.0f,  def.pedFpTiltUp);
		s.pedFpTiltDown      = ClampF(ReadFloatLE(p + 9  * 4), 0.0f,  89.0f,  def.pedFpTiltDown);
		s.vehTpTiltUp        = ClampF(ReadFloatLE(p + 10 * 4), 0.0f,  89.0f,  def.vehTpTiltUp);
		s.vehTpTiltDown      = ClampF(ReadFloatLE(p + 11 * 4), 0.0f,  89.0f,  def.vehTpTiltDown);
		s.vehFpTiltUp        = ClampF(ReadFloatLE(p + 12 * 4), 0.0f,  89.0f,  def.vehFpTiltUp);
		s.vehFpTiltDown      = ClampF(ReadFloatLE(p + 13 * 4), 0.0f,  89.0f,  def.vehFpTiltDown);
		s.pedFpFov           = ClampF(ReadFloatLE(p + 14 * 4), 20.0f, 110.0f, def.pedFpFov);
		(void)ReadFloatLE(p + 15 * 4); // legacy pedFpAimFov slot (removed)
		s.vehTpFov           = ClampF(ReadFloatLE(p + 16 * 4), 20.0f, 110.0f, def.vehTpFov);
		s.vehTpDistance      = ClampF(ReadFloatLE(p + 17 * 4), 0.0f,  4.0f,   def.vehTpDistance);
		s.vehTpPivotScale    = ClampF(ReadFloatLE(p + 18 * 4), 0.0f,  2.0f,   def.vehTpPivotScale);
		s.vehTpZoom          = ClampF(ReadFloatLE(p + 19 * 4), 0.0f,  3.0f,   def.vehTpZoom);
		s.vehFpFov           = ClampF(ReadFloatLE(p + 20 * 4), 20.0f, 110.0f, def.vehFpFov);
		s.vehFpOffsetZ       = ClampF(ReadFloatLE(p + 21 * 4), -0.5f, 0.5f,   def.vehFpOffsetZ);
		s.shakeOnFoot        = ClampF(ReadFloatLE(p + 22 * 4), 0.0f,  300.0f, def.shakeOnFoot);
		s.shakeInVehicle     = ClampF(ReadFloatLE(p + 23 * 4), 0.0f,  300.0f, def.shakeInVehicle);
		s.weaponFovDefault   = ClampF(ReadFloatLE(p + 24 * 4), 0.0f,  110.0f, def.weaponFovDefault);
		s.weaponFovHip       = ClampF(ReadFloatLE(p + 25 * 4), 0.0f,  110.0f, def.weaponFovHip);
		s.weaponFovRpg       = ClampF(ReadFloatLE(p + 26 * 4), 0.0f,  110.0f, def.weaponFovRpg);
		s.weaponFovShotgun   = ClampF(ReadFloatLE(p + 27 * 4), 0.0f,  110.0f, def.weaponFovShotgun);
		s.weaponFovThrown    = ClampF(ReadFloatLE(p + 28 * 4), 0.0f,  110.0f, def.weaponFovThrown);
		s.weaponFovMelee     = ClampF(ReadFloatLE(p + 29 * 4), 0.0f,  110.0f, def.weaponFovMelee);
		s.weaponFovDigiscan  = ClampF(ReadFloatLE(p + 30 * 4), 0.0f,  110.0f, def.weaponFovDigiscan);
		s.weaponFovJerry     = ClampF(ReadFloatLE(p + 31 * 4), 0.0f,  110.0f, def.weaponFovJerry);
		s.weaponFovSniper    = ClampF(ReadFloatLE(p + 32 * 4), 0.0f,  110.0f, def.weaponFovSniper);
		s.weaponFovSniperLow = ClampF(ReadFloatLE(p + 33 * 4), 0.0f,  110.0f, def.weaponFovSniperLow);
		if (isV2)
		{
			s.weaponFovScopeOther = ClampF(ReadFloatLE(p + 34 * 4), 0.0f, 110.0f, def.weaponFovScopeOther);
			s.parachuteFov        = ClampF(ReadFloatLE(p + 35 * 4), 0.0f, 110.0f, def.parachuteFov);
			s.parachuteDistance   = ClampF(ReadFloatLE(p + 36 * 4), 0.0f, 4.0f,   def.parachuteDistance);
			s.vehAimFov           = ClampF(ReadFloatLE(p + 37 * 4), 0.0f, 110.0f, def.vehAimFov);
			s.vehFpOffsetX        = ClampF(ReadFloatLE(p + 38 * 4), -0.5f, 0.5f,  def.vehFpOffsetX);
		}
		else
		{
			s.weaponFovScopeOther = def.weaponFovScopeOther;
			s.parachuteFov        = def.parachuteFov;
			s.parachuteDistance   = def.parachuteDistance;
			s.vehAimFov           = def.vehAimFov;
			s.vehFpOffsetX        = def.vehFpOffsetX;
		}
		return true;
	}
}
