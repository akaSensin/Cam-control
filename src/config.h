/*
	Cam Control - made by sensin
	Reads and writes CamControl.ini next to the plugin.
*/

#pragma once

#include <string>
#include "settings.h"

namespace Config
{
	// Returns true when a config file was found and parsed.
	bool Load(const std::wstring &path, Settings &s);

	// Overwrites the config file with the current settings.
	bool Save(const std::wstring &path, const Settings &s);

	// Encodes camera/preset settings into a shareable ASCII code ("CC1:...").
	std::string ExportCode(const Settings &s);

	// Decodes a shareable preset code into `s` (preserving keybinds, language,
	// and dev probe settings). Returns false if the code is invalid or corrupt.
	bool ImportCode(const std::string &code, Settings &s);
}
