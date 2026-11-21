/*
	Cam Control - made by sensin
	Key name helpers used by the menu.
*/

#pragma once

#include <string>

namespace Keys
{
	// Human readable name for a Win32 virtual-key code ("F7", "NUM 5", "-", "UNBOUND"...).
	std::string Name(int vk);

	// Parses either a human-readable key name ("-", "F8", "UP", "NUM 5", "UNBOUND")
	// or a numeric virtual-key code ("189", "0xBD"). Returns false if unrecognised.
	bool FromName(const std::string &text, int &outVk);

	// Returns the first virtual key that went up this frame, or 0 when none did.
	// Scanning is used so the key bind screen can capture any key without a special hook.
	// The modifier keys, Enter and Backspace are never returned; Escape is, and
	// the caller reads it as "cancel".
	int PollAnyKeyUp();
}
