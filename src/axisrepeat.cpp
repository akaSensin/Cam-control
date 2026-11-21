/*
	Cam Control - made by sensin
*/

#include "axisrepeat.h"

AxisRepeatConfig DefaultAxisRepeatConfig()
{
	AxisRepeatConfig c;
	c.firstDelayMs = 320; // pause before the first repeat
	c.rampSteps = 14;     // repeats before full speed
	c.slowMs = 75;        // interval at the start of a hold
	c.fastMs = 18;        // interval once fully accelerated
	return c;
}

int AdvanceAxisRepeat(AxisRepeatState &st,
	bool posJust, bool negJust,
	bool posHeld, bool negHeld,
	unsigned long now,
	const AxisRepeatConfig &cfg)
{
	// --- edge triggers ---------------------------------------------------
	if (posJust)
	{
		st.dir = 1;
		st.count = 0;
		st.next = now + cfg.firstDelayMs;
		return 1;
	}
	if (negJust)
	{
		st.dir = -1;
		st.count = 0;
		st.next = now + cfg.firstDelayMs;
		return -1;
	}

	// --- repeats while held ----------------------------------------------
	const bool holding = (st.dir == 1 && posHeld) || (st.dir == -1 && negHeld);
	if (holding)
	{
		if (now < st.next)
			return 0;

		st.count++;

		int c = st.count;
		if (c > cfg.rampSteps)
			c = cfg.rampSteps;

		const unsigned long span = (cfg.slowMs > cfg.fastMs) ? (cfg.slowMs - cfg.fastMs) : 0;
		unsigned long interval = cfg.slowMs;
		if (cfg.rampSteps > 0)
			interval = cfg.slowMs - (unsigned long)((span * (unsigned long)c) / (unsigned long)cfg.rampSteps);
		if (interval < 1)
			interval = 1;

		st.next = now + interval;
		return st.dir;
	}

	st.dir = 0;
	return 0;
}
