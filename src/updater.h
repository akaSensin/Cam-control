/*
	Cam Control - made by sensin
	Checks GitHub Releases for a newer version on launch or on demand in-game.
*/

#pragma once

namespace Updater
{
	// Checks GitHub Releases (akaSensin/Cam-control) against currentVersion
	// (e.g. "v0.2") before the game loads. Shows a Yes/No prompt if a newer
	// version is found and opens the release page if the user clicks Yes.
	void CheckOnLaunch(const char *currentVersion);

	// Starts a non-blocking background update check from the in-game menu.
	void StartAsyncCheck(const char *currentVersion);

	// Returns a one-shot status message when an async check finishes, or nullptr.
	const char *TakeStatusMessage();

	// True once a check has found a newer version on GitHub.
	bool UpdateAvailable();

	// Opens the GitHub Releases latest page in the default browser.
	void OpenReleasePage();
}
