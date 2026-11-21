/*
	Cam Control - made by sensin

	Camera metadata layer: fov / follow distance / pivot.

	GTA V keeps the numbers that shape its follow cameras in per-camera
	"metadata" objects, loaded from the game's own camera .meta data. There is no
	native to change them, so the values are written straight into the live
	metadata object of the camera that is on screen.

	Everything here is optional and self-disabling: Init() signature-scans for
	the pieces it needs and, if any is missing (an unknown game build, or another
	mod already patched it), the layer becomes a no-op and the mod keeps working
	through the normal natives.
*/

#pragma once

#include "settings.h"

namespace CamMeta
{
	// One-off signature scan of the game module. Safe to call again.
	void Init();

	// True when the layer found everything it needs on this game build.
	bool Available();

	// Short human readable state, shown in the status line / log.
	const char *Status();

	// The field of view the metadata layer last wrote, and the camera it wrote
	// it to. False when nothing has been written yet. Used by the debug overlay
	// and the FOV cross-check in camera.cpp: comparing this with the FOV the
	// game reports is what shows whether something else has taken the camera
	// over (another mod's menu being the usual suspect).
	bool LastWriteInfo(float &fov, unsigned int &hash);

	// Forgets the per-frame state.
	void Reset();

	// Restores every metadata field modified by the mod back to the original
	// value it held before we first wrote it. Safe to call when disabled.
	void RestoreAll();

	// Writes fov / follow distance / pivot for the camera in use.
	void Tick(const Settings &s, int ped, int veh, bool inVeh, bool firstPerson);

	// Dumps the live metadata object of the camera in use to the log. Used to
	// find the field offset of an option we do not support yet - see the readme.
	void Dump(int ped, int veh, bool inVeh, bool firstPerson);

	// Lists the pool entries whose fov field holds a plausible value. Camera
	// metadata objects carry a field of view; other metadata does not, so this
	// narrows 1600 entries down to the cameras. Run it, change one FOV in the
	// game's own camera settings, run it again - the hash whose value moved is
	// that camera. This is how a hash is verified instead of guessed.
	void ListHashes();

	// Dev aid. While the probe is enabled AND the menu is open, writes
	// Settings::probeValue at Settings::probeOffset inside the live metadata
	// object of the camera in use, and puts the original value back as soon as
	// the menu closes. Reports once per offset when the write is refused, so a
	// probe that does nothing is not a silent mystery.
	void Probe(const Settings &s, int ped, int veh, bool inVeh, bool firstPerson, bool menuOpen);

	// Automatic sweep: walks every offset of the live metadata object, perturbs
	// it, samples the camera through natives, and logs what each offset does -
	// "moves the camera", "rotates it", "changes the FOV", "no effect". Used to
	// build the field table without testing one offset per game restart.
	// Runs only while the menu is open, and restores every value it touches.
	void Sweep(Settings &s, int ped, int veh, bool inVeh, bool firstPerson, bool menuOpen);

	// One-shot status message for the player - "sweep done", "sweep paused" - or
	// nullptr when there is nothing to say. Read it every tick and show it in
	// game, so the sweep does not have to be babysat through the log.
	const char *TakeMessage();
}
