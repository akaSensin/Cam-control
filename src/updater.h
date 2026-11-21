/*
	Cam Control - made by sensin
	Checks GitHub Releases for a newer version before the game finishes loading.
*/

#pragma once

namespace Updater
{
	// Checks GitHub Releases (akaSensin/Cam-control) against currentVersion
	// (e.g. "v0.1"). Shows a Yes/No prompt if a newer version is found and opens
	// the release page in the browser if the user clicks Yes. Safe if offline.
	void CheckOnLaunch(const char *currentVersion);
}
