/*
	Cam Control - made by sensin
	Script entry points.
*/

#pragma once

#include <windows.h>

// Called from DllMain so the plugin can locate its own ini/log files.
void ScriptSetModule(HMODULE module);

// Registered with ScriptHookV.
void ScriptMain();
