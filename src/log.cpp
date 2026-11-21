/*
	Cam Control - made by sensin
*/

#include "log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
	const unsigned long long kMaxLogBytes = 1024ull * 1024ull; // rotate at 1 MB

	HANDLE g_file = INVALID_HANDLE_VALUE;
	CRITICAL_SECTION g_cs;
	bool g_csReady = false;
	std::wstring g_path;

	void WriteRaw(const char *data, int len)
	{
		if (g_file == INVALID_HANDLE_VALUE || len <= 0)
			return;

		DWORD written = 0;
		WriteFile(g_file, data, (DWORD)len, &written, nullptr);
		FlushFileBuffers(g_file);
	}

	// Keeps the log from growing without bound across hundreds of sessions.
	void RotateIfNeeded()
	{
		if (g_file == INVALID_HANDLE_VALUE)
			return;

		LARGE_INTEGER size;
		size.QuadPart = 0;
		if (!GetFileSizeEx(g_file, &size))
			return;

		if ((unsigned long long)size.QuadPart < kMaxLogBytes)
			return;

		CloseHandle(g_file);
		g_file = INVALID_HANDLE_VALUE;

		std::wstring oldPath = g_path + L".old";
		DeleteFileW(oldPath.c_str());
		MoveFileW(g_path.c_str(), oldPath.c_str());

		g_file = CreateFileW(g_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		WriteRaw("[log rotated]\r\n", 15);
	}
}

namespace Log
{
	void Init(const std::wstring &path)
	{
		Close();

		if (!g_csReady)
		{
			InitializeCriticalSection(&g_cs);
			g_csReady = true;
		}

		g_path = path;
		g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (g_file == INVALID_HANDLE_VALUE)
			return;

		LARGE_INTEGER size;
		size.QuadPart = 0;
		GetFileSizeEx(g_file, &size);
		if (size.QuadPart == 0)
		{
			const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF }; // UTF-8 BOM so Notepad detects it
			DWORD written = 0;
			WriteFile(g_file, bom, 3, &written, nullptr);
		}
		SetFilePointer(g_file, 0, nullptr, FILE_END);
	}

	void Close()
	{
		if (g_file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(g_file);
			g_file = INVALID_HANDLE_VALUE;
		}
	}

	const std::wstring &Path()
	{
		return g_path;
	}

	void Line(const char *fmt, ...)
	{
		char body[2048];
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, args);
		va_end(args);

		SYSTEMTIME st;
		GetLocalTime(&st);

		char line[2200];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, body);

		if (g_csReady)
			EnterCriticalSection(&g_cs);

		RotateIfNeeded();
		WriteRaw(line, (int)strlen(line));

		if (g_csReady)
			LeaveCriticalSection(&g_cs);
	}
}
