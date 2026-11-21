/*
	Cam Control - made by sensin
	Simple, thread safe file logger (CamControl.log).
*/

#pragma once

#include <string>

namespace Log
{
	// Opens the log file, rotating it when it grows past the size limit.
	void Init(const std::wstring &path);
	void Close();

	// printf style, always appends CRLF.
	void Line(const char *fmt, ...);

	// Full path of the active log file (empty when logging is disabled).
	const std::wstring &Path();
}
