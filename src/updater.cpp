/*
	Cam Control - made by sensin
	Browser links for the mod page and author page.
*/

#include "updater.h"

#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

namespace
{
	const wchar_t *kModPageUrl = L"https://github.com/akaSensin/Cam-control";
	const wchar_t *kAuthorUrl = L"https://github.com/akaSensin";
}

namespace Updater
{
	void OpenReleasePage()
	{
		ShellExecuteW(nullptr, L"open", kModPageUrl, nullptr, nullptr, SW_SHOWNORMAL);
	}

	void OpenAuthorPage()
	{
		ShellExecuteW(nullptr, L"open", kAuthorUrl, nullptr, nullptr, SW_SHOWNORMAL);
	}
}
