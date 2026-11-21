/*
	Cam Control - made by sensin
*/

#include "keys.h"
#include "keyboard.h"

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace Keys
{
	std::string Name(int vk)
	{
		if (vk <= 0)
			return "UNBOUND";

		// letters
		if (vk >= 'A' && vk <= 'Z')
		{
			char b[2] = { (char)vk, 0 };
			return std::string(b);
		}
		if (vk >= '0' && vk <= '9')
		{
			char b[2] = { (char)vk, 0 };
			return std::string(b);
		}
		if (vk >= VK_F1 && vk <= VK_F24)
		{
			char b[16];
			_snprintf_s(b, sizeof(b), _TRUNCATE, "F%d", vk - VK_F1 + 1);
			return std::string(b);
		}
		if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
		{
			char b[16];
			_snprintf_s(b, sizeof(b), _TRUNCATE, "NUM %d", vk - VK_NUMPAD0);
			return std::string(b);
		}

		switch (vk)
		{
		case VK_BACK:       return "BACKSPACE";
		case VK_TAB:        return "TAB";
		case VK_RETURN:     return "ENTER";
		case VK_SHIFT:      return "SHIFT";
		case VK_CONTROL:    return "CTRL";
		case VK_MENU:       return "ALT";
		case VK_PAUSE:      return "PAUSE";
		case VK_CAPITAL:    return "CAPSLOCK";
		case VK_ESCAPE:     return "ESCAPE";
		case VK_SPACE:      return "SPACE";
		case VK_PRIOR:      return "PGUP";
		case VK_NEXT:       return "PGDOWN";
		case VK_END:        return "END";
		case VK_HOME:       return "HOME";
		case VK_LEFT:       return "LEFT";
		case VK_UP:         return "UP";
		case VK_RIGHT:      return "RIGHT";
		case VK_DOWN:       return "DOWN";
		case VK_INSERT:     return "INSERT";
		case VK_DELETE:     return "DELETE";
		case VK_LWIN:       return "LWIN";
		case VK_RWIN:       return "RWIN";
		case VK_NUMLOCK:    return "NUMLOCK";
		case VK_SCROLL:     return "SCROLLLOCK";
		case VK_MULTIPLY:   return "NUM *";
		case VK_ADD:        return "NUM +";
		case VK_SUBTRACT:   return "NUM -";
		case VK_DECIMAL:    return "NUM .";
		case VK_DIVIDE:     return "NUM /";
		case VK_OEM_1:      return "OEM_1";
		case VK_OEM_PLUS:   return "OEM_PLUS";
		case VK_OEM_COMMA:  return "OEM_COMMA";
		case VK_OEM_MINUS:  return "-";
		case VK_OEM_PERIOD: return "OEM_PERIOD";
		case VK_OEM_2:      return "OEM_2";
		case VK_OEM_3:      return "OEM_3";
		case VK_OEM_4:      return "OEM_4";
		case VK_OEM_5:      return "OEM_5";
		case VK_OEM_6:      return "OEM_6";
		case VK_OEM_7:      return "OEM_7";
		case VK_OEM_8:      return "OEM_8";
		default: break;
		}

		char b[32];
		_snprintf_s(b, sizeof(b), _TRUNCATE, "VK 0x%02X", vk);
		return std::string(b);
	}

	int PollAnyKeyUp()
	{
		// Skipping the mouse buttons and the modifier keys keeps the bind screen
		// from latching onto something the player did not mean to press.
		//
		// Enter and Backspace are skipped as well. Both are the menu's permanent
		// fallbacks (Enter selects, Backspace goes back), so binding them to
		// another row - above all to the menu-open key, where Enter would then
		// both open the menu and select - can only ever break the menu. Escape
		// is NOT skipped: the bind screen uses it to cancel.
		for (int vk = VK_BACK; vk <= 0xFE; vk++)
		{
			if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
				vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL ||
				vk == VK_RCONTROL || vk == VK_LMENU || vk == VK_RMENU ||
				vk == VK_LWIN || vk == VK_RWIN ||
				vk == VK_RETURN || vk == VK_BACK)
				continue;

			if (IsKeyJustUp((DWORD)vk, true))
				return vk;
		}
		return 0;
	}

	bool FromName(const std::string &text, int &outVk)
	{
		size_t a = 0, b = text.size();
		while (a < b && (unsigned char)text[a] <= ' ') a++;
		while (b > a && (unsigned char)text[b - 1] <= ' ') b--;
		if (a == b)
			return false;

		std::string u;
		u.reserve(b - a);
		for (size_t i = a; i < b; i++)
			u.push_back((char)toupper((unsigned char)text[i]));

		if (u == "UNBOUND" || u == "NONE" || u == "OFF" || u == "0") { outVk = 0; return true; }

		if (u.size() == 1)
		{
			const char c = u[0];
			if (c >= 'A' && c <= 'Z') { outVk = (int)c; return true; }
			if (c >= '1' && c <= '9') { outVk = (int)c; return true; }
			switch (c)
			{
			case '-': outVk = VK_OEM_MINUS; return true;
			case '=': case '+': outVk = VK_OEM_PLUS; return true;
			case ',': outVk = VK_OEM_COMMA; return true;
			case '.': outVk = VK_OEM_PERIOD; return true;
			case ';': outVk = VK_OEM_1; return true;
			case '/': outVk = VK_OEM_2; return true;
			case '`': outVk = VK_OEM_3; return true;
			case '[': outVk = VK_OEM_4; return true;
			case '\\': outVk = VK_OEM_5; return true;
			case ']': outVk = VK_OEM_6; return true;
			case '\'': outVk = VK_OEM_7; return true;
			default: break;
			}
		}

		if (u == "KEY_0" || u == "KEY 0") { outVk = '0'; return true; }

		// Function keys F1..F24
		if (u.size() >= 2 && u[0] == 'F' && isdigit((unsigned char)u[1]))
		{
			char *end = nullptr;
			long fn = strtol(u.c_str() + 1, &end, 10);
			if (end != u.c_str() + 1 && *end == '\0' && fn >= 1 && fn <= 24)
			{
				outVk = VK_F1 + (int)fn - 1;
				return true;
			}
		}

		// Numpad keys ("NUM 0".."NUM 9", "NUM0".."NUM9", "NUMPAD0".."NUMPAD9", "NUM *")
		const char *numRest = nullptr;
		if (u.compare(0, 6, "NUMPAD") == 0) numRest = u.c_str() + 6;
		else if (u.compare(0, 4, "NUM ") == 0) numRest = u.c_str() + 4;
		else if (u.compare(0, 3, "NUM") == 0 && u.size() == 4) numRest = u.c_str() + 3;
		if (numRest && numRest[0] != '\0' && numRest[1] == '\0')
		{
			if (numRest[0] >= '0' && numRest[0] <= '9') { outVk = VK_NUMPAD0 + (numRest[0] - '0'); return true; }
			if (numRest[0] == '*') { outVk = VK_MULTIPLY; return true; }
			if (numRest[0] == '+') { outVk = VK_ADD; return true; }
			if (numRest[0] == '-') { outVk = VK_SUBTRACT; return true; }
			if (numRest[0] == '.') { outVk = VK_DECIMAL; return true; }
			if (numRest[0] == '/') { outVk = VK_DIVIDE; return true; }
		}

		if (u == "BACKSPACE" || u == "BACK") { outVk = VK_BACK; return true; }
		if (u == "TAB") { outVk = VK_TAB; return true; }
		if (u == "ENTER" || u == "RETURN") { outVk = VK_RETURN; return true; }
		if (u == "SHIFT") { outVk = VK_SHIFT; return true; }
		if (u == "CTRL" || u == "CONTROL") { outVk = VK_CONTROL; return true; }
		if (u == "ALT") { outVk = VK_MENU; return true; }
		if (u == "PAUSE") { outVk = VK_PAUSE; return true; }
		if (u == "CAPSLOCK" || u == "CAPS") { outVk = VK_CAPITAL; return true; }
		if (u == "ESCAPE" || u == "ESC") { outVk = VK_ESCAPE; return true; }
		if (u == "SPACE" || u == "SPACEBAR") { outVk = VK_SPACE; return true; }
		if (u == "PGUP" || u == "PAGEUP") { outVk = VK_PRIOR; return true; }
		if (u == "PGDOWN" || u == "PAGEDOWN" || u == "PGDN") { outVk = VK_NEXT; return true; }
		if (u == "END") { outVk = VK_END; return true; }
		if (u == "HOME") { outVk = VK_HOME; return true; }
		if (u == "LEFT") { outVk = VK_LEFT; return true; }
		if (u == "UP") { outVk = VK_UP; return true; }
		if (u == "RIGHT") { outVk = VK_RIGHT; return true; }
		if (u == "DOWN") { outVk = VK_DOWN; return true; }
		if (u == "INSERT" || u == "INS") { outVk = VK_INSERT; return true; }
		if (u == "DELETE" || u == "DEL") { outVk = VK_DELETE; return true; }
		if (u == "LWIN") { outVk = VK_LWIN; return true; }
		if (u == "RWIN") { outVk = VK_RWIN; return true; }
		if (u == "NUMLOCK") { outVk = VK_NUMLOCK; return true; }
		if (u == "SCROLLLOCK") { outVk = VK_SCROLL; return true; }
		if (u == "MINUS" || u == "OEM_MINUS") { outVk = VK_OEM_MINUS; return true; }
		if (u == "PLUS" || u == "EQUALS" || u == "OEM_PLUS") { outVk = VK_OEM_PLUS; return true; }
		if (u == "COMMA" || u == "OEM_COMMA") { outVk = VK_OEM_COMMA; return true; }
		if (u == "PERIOD" || u == "OEM_PERIOD") { outVk = VK_OEM_PERIOD; return true; }
		if (u == "OEM_1") { outVk = VK_OEM_1; return true; }
		if (u == "OEM_2") { outVk = VK_OEM_2; return true; }
		if (u == "OEM_3") { outVk = VK_OEM_3; return true; }
		if (u == "OEM_4") { outVk = VK_OEM_4; return true; }
		if (u == "OEM_5") { outVk = VK_OEM_5; return true; }
		if (u == "OEM_6") { outVk = VK_OEM_6; return true; }
		if (u == "OEM_7") { outVk = VK_OEM_7; return true; }
		if (u == "OEM_8") { outVk = VK_OEM_8; return true; }

		// Fallback: "VK 0xBD" or raw decimal/hex virtual-key code (e.g. "189", "0xBD").
		const char *numStr = (u.compare(0, 3, "VK ") == 0) ? (u.c_str() + 3) : u.c_str();
		char *end = nullptr;
		long vk = strtol(numStr, &end, 0);
		if (end != numStr && *end == '\0' && vk >= 0 && vk <= 254)
		{
			outVk = (int)vk;
			return true;
		}

		return false;
	}
}
