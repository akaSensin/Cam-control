/*
	Cam Control - made by sensin

	DirectWrite text overlay.

	GTA V's own text engine is left-to-right and Latin/Cyrillic only, so it
	cannot draw Arabic (right-to-left, contextual letter shaping) or Chinese and
	Japanese (the game font simply has no such glyphs).

	This module draws those languages itself:

	    script thread                     render thread (Present callback)
	    -------------                     -------------------------------
	    AddText(...)  --queue-->  rasterize with DirectWrite into a WIC bitmap
	                              (Direct2D SOFTWARE rendering - GTA's D3D11
	                               device is created without BGRA support, so
	                               Direct2D cannot bind to it)
	                            -> upload the pixels into a D3D11 texture
	                            -> cache by (text, size, colour)
	                            -> draw one textured quad per item

	Everything is best-effort: if any piece fails to initialise, the overlay
	switches itself off and logs why. It never takes the game down with it.

	Latin and Cyrillic languages keep using the game's own text renderer and
	never come through here - see lang.cpp for which language takes which path.
*/

#pragma once

#include <string>

namespace DxText
{
	enum Align
	{
		AlignLeft = 0,
		AlignRight = 1,
		AlignCenter = 2
	};

	// Registers / unregisters the swapchain Present callback.
	// Register() must be called from DllMain (DLL_PROCESS_ATTACH).
	void Register();
	void Unregister();

	// Called from the render thread. Draws and clears the queue.
	void OnPresent(void *swapChain);

	// ---- script thread -------------------------------------------------

	// Queues one string for this frame. Coordinates are the same 0..1 screen
	// space the game's natives use. `scale` is a GTA text scale, converted to
	// pixels here so a translated line keeps the size it had on the native path.
	// For AlignRight, x is the right edge; for AlignCenter, the centre.
	void AddText(const std::string &utf8, float x, float y, float scale, const int col[4], int align);

	// Clears anything drawn without being re-queued this frame.
	void BeginFrame();

	// True once the overlay has given up (logged once, never retried).
	bool Failed();
}
