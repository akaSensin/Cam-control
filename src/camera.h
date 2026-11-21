/*
	Cam Control - made by sensin
	Applies the camera settings every game tick.
*/

#pragma once

#include "settings.h"

namespace CameraControl
{
	// Called once per script tick.
	void Tick(Settings &s, bool menuOpen);

	// Releases anything we are holding (mod disabled, cutscene, script unload).
	void Shutdown();

	// Short human readable state for the menu and the debug overlay.
	const char *StatusText();

	// Re-baselines the internal state. Call after a reset.
	void ResetLook();

	// True once after the mod stopped forcing a view mode because the player
	// changed it (pressing V). Lets the caller save the updated setting.
	bool ConsumeForceReleased();

	// Writes the live metadata object of the camera on screen to the log, so the
	// field offset of a new option can be found by comparing two dumps.
	void DumpMetadata();
}
