/*
		THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
					http://dev-c.com
				(C) Alexander Blade 2015

	Extended with key-down edge detection. The SDK only ships IsKeyJustUp(),
	which fires when the key is RELEASED. That is fine for a one-shot trainer
	toggle, but useless for menu navigation: holding a key produced no repeat,
	so sliders had to be tapped one step at a time.
*/

#include "keyboard.h"

const int KEYS_SIZE = 255;

struct
{
	DWORD time;
	BOOL isWithAlt;
	BOOL wasDownBefore;
	BOOL isUpNow;
} keyStates[KEYS_SIZE];

void OnKeyboardMessage(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended, BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow)
{
	if (key < KEYS_SIZE)
	{
		keyStates[key].time = GetTickCount();
		keyStates[key].isWithAlt = isWithAlt;
		keyStates[key].wasDownBefore = wasDownBefore;
		keyStates[key].isUpNow = isUpNow;
	}
}

const int NOW_PERIOD = 100, MAX_DOWN = 5000; // ms

bool IsKeyDown(DWORD key)
{
	return (key < KEYS_SIZE) ? ((GetTickCount() < keyStates[key].time + MAX_DOWN) && !keyStates[key].isUpNow) : false;
}

bool IsKeyUp(DWORD key)
{
	return (key < KEYS_SIZE) ? (keyStates[key].isUpNow != 0) : false;
}

bool IsKeyJustDown(DWORD key, bool exclusive)
{
	if (key >= KEYS_SIZE)
		return false;

	// A genuine press: the key is down now and was NOT down in the previous
	// message. OS auto-repeat messages report wasDownBefore = TRUE, so they are
	// filtered out here and handled as "held" instead.
	bool b = (GetTickCount() < keyStates[key].time + NOW_PERIOD)
		&& !keyStates[key].isUpNow
		&& !keyStates[key].wasDownBefore;

	if (b && exclusive)
	{
		// Consume the edge WITHOUT breaking hold detection: pretend the key has
		// been down for a while, so IsKeyJustDown() stays quiet while it is held
		// but IsKeyDown() keeps returning true.
		keyStates[key].wasDownBefore = TRUE;
	}
	return b;
}

bool IsKeyJustUp(DWORD key, bool exclusive)
{
	bool b = (key < KEYS_SIZE) ? (GetTickCount() < keyStates[key].time + NOW_PERIOD && keyStates[key].isUpNow) : false;
	if (b && exclusive)
		ResetKeyState(key);
	return b;
}

void ResetKeyState(DWORD key)
{
	if (key < KEYS_SIZE)
		memset(&keyStates[key], 0, sizeof(keyStates[0]));
}
