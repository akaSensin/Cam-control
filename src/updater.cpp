/*
	Cam Control - made by sensin
	Update check using WinHTTP (GitHub Releases API).
*/

#include "updater.h"
#include "log.h"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
	const wchar_t *kReleaseUrl = L"https://github.com/akaSensin/Cam-control/releases/latest";

	volatile LONG g_checking = 0;
	volatile LONG g_msgReady = 0;
	volatile LONG g_updateAvailable = 0;
	char g_msgBuf[160] = { 0 };
	char g_asyncCurrentVer[32] = { 0 };

	bool ParseVersion(const std::string &s, int &major, int &minor, int &patch)
	{
		major = minor = patch = 0;
		const char *p = s.c_str();
		while (*p && (*p == 'v' || *p == 'V' || *p == ' '))
			p++;
		if (*p < '0' || *p > '9')
			return false;
		const int n = sscanf_s(p, "%d.%d.%d", &major, &minor, &patch);
		return n >= 1;
	}

	bool IsNewerVersion(const std::string &remote, const char *local)
	{
		int rMaj = 0, rMin = 0, rPat = 0;
		int lMaj = 0, lMin = 0, lPat = 0;
		if (!ParseVersion(remote, rMaj, rMin, rPat))
			return false;
		if (!ParseVersion(local ? local : "", lMaj, lMin, lPat))
			return false;

		if (rMaj != lMaj) return rMaj > lMaj;
		if (rMin != lMin) return rMin > lMin;
		return rPat > lPat;
	}

	bool ExtractJsonString(const std::string &json, const char *key, std::string &out)
	{
		const std::string pattern = std::string("\"") + key + "\"";
		size_t pos = json.find(pattern);
		if (pos == std::string::npos)
			return false;
		pos = json.find(':', pos + pattern.size());
		if (pos == std::string::npos)
			return false;
		pos = json.find('"', pos + 1);
		if (pos == std::string::npos)
			return false;
		const size_t end = json.find('"', pos + 1);
		if (end == std::string::npos)
			return false;
		out = json.substr(pos + 1, end - (pos + 1));
		return !out.empty();
	}

	bool FetchLatestReleaseJson(std::string &outBody)
	{
		outBody.clear();
		HINTERNET hSession = WinHttpOpen(L"CamControl-Updater/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
			return false;

		WinHttpSetTimeouts(hSession, 2500, 2500, 2500, 2500);

		HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
			L"/repos/akaSensin/Cam-control/releases/latest",
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		const wchar_t *headers = L"Accept: application/vnd.github+json\r\n";
		BOOL ok = WinHttpSendRequest(hRequest, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (ok)
			ok = WinHttpReceiveResponse(hRequest, nullptr);

		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		if (ok)
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

		if (ok && status == 200)
		{
			DWORD avail = 0;
			while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0)
			{
				if (outBody.size() + avail > 65536)
					break;
				std::string chunk(avail, '\0');
				DWORD read = 0;
				if (WinHttpReadData(hRequest, &chunk[0], avail, &read) && read > 0)
					outBody.append(chunk.data(), read);
				else
					break;
			}
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return status == 200 && !outBody.empty();
	}

	enum CheckResult { CHECK_FAILED, CHECK_UP_TO_DATE, CHECK_UPDATE_FOUND };

	CheckResult RunCheck(const char *currentVersion, std::string &outTag)
	{
		outTag.clear();
		std::string body;
		if (!FetchLatestReleaseJson(body))
			return CHECK_FAILED;

		if (!ExtractJsonString(body, "tag_name", outTag))
			return CHECK_FAILED;

		if (IsNewerVersion(outTag, currentVersion))
		{
			InterlockedExchange(&g_updateAvailable, 1);
			return CHECK_UPDATE_FOUND;
		}
		InterlockedExchange(&g_updateAvailable, 0);
		return CHECK_UP_TO_DATE;
	}

	DWORD WINAPI AsyncCheckThread(LPVOID)
	{
		std::string tag;
		const CheckResult res = RunCheck(g_asyncCurrentVer, tag);
		if (res == CHECK_UPDATE_FOUND)
		{
			Log::Line("updater: new version %s available (current %s)", tag.c_str(), g_asyncCurrentVer);
			_snprintf_s(g_msgBuf, sizeof(g_msgBuf), _TRUNCATE,
				"Update %s available! Select 'Open Update Page' in Settings.", tag.c_str());
		}
		else if (res == CHECK_UP_TO_DATE)
		{
			Log::Line("updater: up to date (%s)", g_asyncCurrentVer);
			_snprintf_s(g_msgBuf, sizeof(g_msgBuf), _TRUNCATE,
				"Cam Control is up to date (%s).", g_asyncCurrentVer);
		}
		else
		{
			Log::Line("updater: check failed (offline or no releases)");
			_snprintf_s(g_msgBuf, sizeof(g_msgBuf), _TRUNCATE,
				"Update check failed (offline or no releases).");
		}
		InterlockedExchange(&g_msgReady, 1);
		InterlockedExchange(&g_checking, 0);
		return 0;
	}
}

namespace Updater
{
	void CheckOnLaunch(const char *currentVersion)
	{
		std::string tag;
		const CheckResult res = RunCheck(currentVersion, tag);
		if (res == CHECK_FAILED)
		{
			Log::Line("updater: no release info available (offline or no releases yet)");
			return;
		}
		if (res == CHECK_UP_TO_DATE)
		{
			Log::Line("updater: up to date (%s)", currentVersion ? currentVersion : "");
			return;
		}

		Log::Line("updater: new version %s available (current %s)",
			tag.c_str(), currentVersion ? currentVersion : "");

		wchar_t msg[512];
		_snwprintf_s(msg, _TRUNCATE,
			L"A new version of Cam Control (%S) is available (current: %S).\n\n"
			L"Open the GitHub release page to download it?",
			tag.c_str(), currentVersion ? currentVersion : "unknown");

		const int choice = MessageBoxW(nullptr, msg, L"Cam Control - Update Available",
			MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);

		if (choice == IDYES)
			OpenReleasePage();
	}

	void StartAsyncCheck(const char *currentVersion)
	{
		if (InterlockedCompareExchange(&g_checking, 1, 0) != 0)
			return;

		_snprintf_s(g_asyncCurrentVer, sizeof(g_asyncCurrentVer), _TRUNCATE, "%s",
			currentVersion ? currentVersion : "");
		HANDLE h = CreateThread(nullptr, 0, AsyncCheckThread, nullptr, 0, nullptr);
		if (h)
			CloseHandle(h);
		else
			InterlockedExchange(&g_checking, 0);
	}

	const char *TakeStatusMessage()
	{
		if (InterlockedCompareExchange(&g_msgReady, 0, 1) != 1)
			return nullptr;
		return g_msgBuf;
	}

	bool UpdateAvailable()
	{
		return InterlockedCompareExchange(&g_updateAvailable, 0, 0) != 0;
	}

	void OpenReleasePage()
	{
		ShellExecuteW(nullptr, L"open", kReleaseUrl, nullptr, nullptr, SW_SHOWNORMAL);
	}
}
