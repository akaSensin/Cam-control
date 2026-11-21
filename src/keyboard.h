/*
		THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
					http://dev-c.com
				(C) Alexander Blade 2015

	Extended with key-down edge detection. The SDK only ships IsKeyJustUp(),
	which fires when the key is RELEASED - fine for a one-shot trainer toggle,
	but useless for menu navigation because holding a key produces no repeat.
*/

#pragma once

#include <windows.h>

// parameters are the same as with aru's ScriptHook for IV
void OnKeyboardMessage(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended, BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow);

bool IsKeyDown(DWORD key);

// True on the frame the key goes down (not on OS auto-repeat). When exclusive is
// true the edge is consumed, but IsKeyDown() keeps reporting the key, so a single
// call gives you both "pressed now" and "still held".
bool IsKeyJustDown(DWORD key, bool exclusive = true);

// True on the frame the key comes up.
bool IsKeyJustUp(DWORD key, bool exclusive = true);

bool IsKeyUp(DWORD key);

void ResetKeyState(DWORD key);
