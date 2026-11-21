/*
	Cam Control - made by sensin
*/

#include "ui.h"

#include "natives.h"
#include "lang.h"
#include "dxtext.h"

namespace Ui
{
	void FillRect(float left, float top, float w, float h, const int col[4])
	{
		GRAPHICS::DRAW_RECT(left + w * 0.5f, top + h * 0.5f, w, h, col[0], col[1], col[2], col[3]);
	}

	void TextStyle(float scale, const int col[4], int font)
	{
		UI::SET_TEXT_FONT(font);
		UI::SET_TEXT_SCALE(0.0f, scale);
		UI::SET_TEXT_COLOUR(col[0], col[1], col[2], col[3]);
		UI::SET_TEXT_CENTRE(0);
		UI::SET_TEXT_RIGHT_JUSTIFY(0);
		UI::SET_TEXT_WRAP(0.0f, 1.0f);
		UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 170);
		UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
	}

	void TextLeft(const std::string &text, float x, float y, float scale, const int col[4], int font)
	{
		// Arabic / Chinese / Japanese cannot go through the game's text engine,
		// so those are handed to the DirectWrite overlay instead. If that overlay
		// failed to initialise we still draw natively: the glyphs will be missing,
		// but a menu with boxes beats a menu with no text at all.
		if (Lang::NeedsCustomRender() && !DxText::Failed())
		{
			DxText::AddText(text, x, y, scale, col, DxText::AlignLeft);
			return;
		}

		TextStyle(scale, col, font);
		UI::_SET_TEXT_ENTRY("STRING");
		UI::_ADD_TEXT_COMPONENT_STRING(const_cast<char *>(text.c_str()));
		UI::_DRAW_TEXT(x, y);
	}

	void TextRight(const std::string &text, float rightEdge, float y, float scale, const int col[4], int font)
	{
		if (Lang::NeedsCustomRender() && !DxText::Failed())
		{
			// The overlay right-aligns against the same edge the native path uses.
			DxText::AddText(text, rightEdge, y, scale, col, DxText::AlignRight);
			return;
		}

		TextStyle(scale, col, font);
		UI::SET_TEXT_RIGHT_JUSTIFY(1);
		UI::SET_TEXT_WRAP(0.0f, rightEdge);
		UI::_SET_TEXT_ENTRY("STRING");
		UI::_ADD_TEXT_COMPONENT_STRING(const_cast<char *>(text.c_str()));
		UI::_DRAW_TEXT(0.0f, y);
		UI::SET_TEXT_RIGHT_JUSTIFY(0);
		UI::SET_TEXT_WRAP(0.0f, 1.0f);
	}

	void ScreenResolution(int &w, int &h)
	{
		w = 1920;
		h = 1080;
		GRAPHICS::GET_SCREEN_RESOLUTION(&w, &h);
	}
}
