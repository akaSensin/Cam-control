/*
	Cam Control - made by sensin
	Thin wrappers around the game's 2D drawing natives (kept in one place so the
	menu and the overlays share exactly the same look).
*/

#pragma once

#include <string>

namespace Ui
{
	// Screen space is 0..1 on both axes, matching the natives.
	void FillRect(float left, float top, float w, float h, const int col[4]);

	void TextStyle(float scale, const int col[4], int font);
	void TextLeft(const std::string &text, float x, float y, float scale, const int col[4], int font = 0);
	void TextRight(const std::string &text, float rightEdge, float y, float scale, const int col[4], int font = 0);

	void ScreenResolution(int &w, int &h);
}
