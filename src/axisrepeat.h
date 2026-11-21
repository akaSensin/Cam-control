/*
	Cam Control - made by sensin

	Pure held-key repeat scheduler.

	Kept free of any game or Windows dependency on purpose: the whole point of
	this file is that the repeat behaviour can be unit tested outside the game.

	Behaviour: one step on the key-down edge, then a pause, then repeats that
	speed up from `slowMs` to `fastMs` over `rampSteps` steps.
*/

#pragma once

struct AxisRepeatState
{
	int dir;              // current direction: -1, 0 or +1
	unsigned long next;   // tick (ms) at which the next repeat may fire
	int count;            // repeats fired during the current hold
};

// Tuning shared by the menu and the tests.
struct AxisRepeatConfig
{
	unsigned long firstDelayMs;
	int rampSteps;
	unsigned long slowMs;
	unsigned long fastMs;
};

AxisRepeatConfig DefaultAxisRepeatConfig();

// Advances the scheduler by one frame.
//   posJust / negJust - a fresh key-down edge happened this frame
//   posHeld / negHeld - the key is still down
// Returns -1, 0 or +1: the number of steps to apply this frame.
int AdvanceAxisRepeat(AxisRepeatState &st,
	bool posJust, bool negJust,
	bool posHeld, bool negHeld,
	unsigned long now,
	const AxisRepeatConfig &cfg = DefaultAxisRepeatConfig());
