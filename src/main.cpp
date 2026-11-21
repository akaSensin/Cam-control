/*
	THIS FILE IS A PART OF GTA V SCRIPT HOOK SDK
				http://dev-c.com
			(C) Alexander Blade 2015

	Cam Control - made by sensin
*/

#include "main.h"
#include "script.h"
#include "keyboard.h"
#include "dxtext.h"

BOOL APIENTRY DllMain(HMODULE hInstance, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		ScriptSetModule(hInstance);
		scriptRegister(hInstance, ScriptMain);
		keyboardHandlerRegister(OnKeyboardMessage);
		// The swapchain callback has to be registered on attach, so the Arabic /
		// CJK overlay can hook Present. It is a no-op for Latin and Cyrillic.
		DxText::Register();
		break;
	case DLL_PROCESS_DETACH:
		DxText::Unregister();
		scriptUnregister(hInstance);
		keyboardHandlerUnregister(OnKeyboardMessage);
		break;
	}
	return TRUE;
}
