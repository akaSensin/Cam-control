/*
	Cam Control - made by sensin
	Menu rendering and input.

	Input model
	-----------
	Navigation and value changes trigger on the key-DOWN edge and then repeat
	while the key is held, with acceleration. The ScriptHookV SDK only provides
	IsKeyJustUp() (fires on release), which is why holding a key used to do
	nothing and sliders had to be tapped repeatedly. IsKeyJustDown() from
	keyboard.h fixes that.

	Theme
	-----
	The accent colour comes from the settings, so the menu can be any colour.
	Text drawn on top of the accent is switched between black and white
	automatically from the accent's luminance, so it stays readable.
*/

#include "menu.h"
#include "lang.h"
#include "axisrepeat.h"
#include "keys.h"
#include "keyboard.h"
#include "log.h"
#include "ui.h"

#include "natives.h"

#include <windows.h>
#include <cmath>
#include <cstdio>

namespace
{
	// ---- layout (screen space, 0..1) ------------------------------------
	const float PANEL_X = 0.020f;
	const float PANEL_W = 0.252f;
	const float PANEL_Y = 0.098f;
	const float HEADER_H = 0.082f;
	const float PAGEBAR_H = 0.028f;
	const float ROW_H = 0.0345f;
	const float FOOTER_H = 0.032f;
	const int   MAX_ROWS = 14;

	// ---- hold repeat tuning ---------------------------------------------
	// The real values live in axisrepeat.cpp so the behaviour can be unit tested.

	// ---- neutral theme parts (independent of the accent) -----------------
	const int COL_PANEL_BG[4] = { 0, 0, 0, 208 };
	const int COL_ROW_BG[4] = { 255, 255, 255, 14 };
	const int COL_HINT_BG[4] = { 0, 0, 0, 225 };
	const int COL_TEXT[4] = { 255, 255, 255, 255 };
	const int COL_TEXT_DIM[4] = { 176, 176, 176, 255 };
	const int COL_TRACK[4] = { 255, 255, 255, 70 };
	const int COL_TRACK_ACTIVE[4] = { 0, 0, 0, 95 };

	// Colours derived from the user's accent each frame.
	struct Theme
	{
		int accent[4];      // solid accent
		int accentSoft[4];  // accent used for slider fill on idle rows
		int onAccent[4];    // text drawn on the accent (auto contrast)
		int dimOnAccent[4]; // secondary text on the accent
	};

	Theme BuildTheme(const Settings &s)
	{
		Theme t;

		int r = s.themeR, g = s.themeG, b = s.themeB;
		if (r < 0) r = 0; if (r > 255) r = 255;
		if (g < 0) g = 0; if (g > 255) g = 255;
		if (b < 0) b = 0; if (b > 255) b = 255;

		t.accent[0] = r; t.accent[1] = g; t.accent[2] = b; t.accent[3] = 244;
		t.accentSoft[0] = r; t.accentSoft[1] = g; t.accentSoft[2] = b; t.accentSoft[3] = 215;

		// Perceived luminance decides whether black or white text is readable.
		const float lum = 0.299f * (float)r + 0.587f * (float)g + 0.114f * (float)b;
		if (lum >= 150.0f)
		{
			t.onAccent[0] = 8; t.onAccent[1] = 8; t.onAccent[2] = 8; t.onAccent[3] = 255;
			t.dimOnAccent[0] = 60; t.dimOnAccent[1] = 60; t.dimOnAccent[2] = 60; t.dimOnAccent[3] = 255;
		}
		else
		{
			t.onAccent[0] = 255; t.onAccent[1] = 255; t.onAccent[2] = 255; t.onAccent[3] = 255;
			t.dimOnAccent[0] = 205; t.dimOnAccent[1] = 205; t.dimOnAccent[2] = 205; t.dimOnAccent[3] = 255;
		}
		return t;
	}

	std::string FormatNumber(float v, float step)
	{
		char buf[48];
		float av = fabsf(step);
		if (av >= 1.0f)
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.0f", v);
		else if (av >= 0.1f)
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.2f", v);
		else
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.3f", v);
		return std::string(buf);
	}

	void Beep(const char *sound)
	{
		AUDIO::PLAY_SOUND_FRONTEND(-1, const_cast<char *>(sound), "HUD_FRONTEND_DEFAULT_SOUNDSET", 0);
	}
}

MenuSystem::MenuSystem()
{
}

int MenuSystem::AddPage(const std::string &title)
{
	MenuPage p;
	p.title = title;
	m_pages.push_back(p);
	return (int)m_pages.size() - 1;
}

void MenuSystem::Toggle(int page, const char *text, bool *flag)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Toggle;
	it.text = text;
	it.flag = flag;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Slider(int page, const char *text, float *value, float vmin, float vmax, float step, const char *unit)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Slider;
	it.text = text;
	it.value = value;
	it.vmin = vmin;
	it.vmax = vmax;
	it.vstep = step;
	if (unit)
		it.unit = unit;
	m_pages[page].items.push_back(it);
}

void MenuSystem::SliderInt(int page, const char *text, int *value, int vmin, int vmax, int step)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::SliderInt;
	it.text = text;
	it.ivalue = value;
	it.intSlider = true;
	it.vmin = (float)vmin;
	it.vmax = (float)vmax;
	it.istep = step > 0 ? step : 1;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Option(int page, const char *text, int *value, const std::vector<std::string> &options)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Option;
	it.text = text;
	it.ivalue = value;
	it.options.reserve(options.size());
	for (size_t oi = 0; oi < options.size(); oi++)
		it.options.push_back(Lang::Tr(options[oi].c_str()));
	m_pages[page].items.push_back(it);
}

void MenuSystem::Submenu(int page, const char *text, int targetPage)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Submenu;
	it.text = text;
	it.submenu = targetPage;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Action(int page, const char *text, std::function<void()> action)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Action;
	it.text = text;
	it.action = action;
	m_pages[page].items.push_back(it);
}

void MenuSystem::KeyBind(int page, const char *text, int *vk)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::KeyBind;
	it.text = text;
	it.ivalue = vk;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Label(int page, const char *text)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Label;
	it.text = text;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Label(int page, std::function<std::string()> textFn)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Label;
	it.textFn = textFn;
	m_pages[page].items.push_back(it);
}

void MenuSystem::Separator(int page)
{
	if (page < 0 || page >= (int)m_pages.size()) return;
	MenuItem it;
	it.kind = ItemKind::Separator;
	m_pages[page].items.push_back(it);
}

MenuPage *MenuSystem::Current()
{
	if (m_page < 0 || m_page >= (int)m_pages.size())
		return nullptr;
	return &m_pages[m_page];
}

void MenuSystem::Open(int page)
{
	if (page >= 0 && page < (int)m_pages.size())
		m_page = page;
	m_open = true;
	m_sel = 0;
	m_scroll = 0;
	m_repV = AxisRepeatState{ 0, 0, 0 };
	m_repH = AxisRepeatState{ 0, 0, 0 };
	m_listening = false;
	m_listenTarget = nullptr;

	// land on the first selectable row
	MenuPage *p = Current();
	if (p)
	{
		for (size_t i = 0; i < p->items.size(); i++)
		{
			if (p->items[i].Selectable()) { m_sel = (int)i; break; }
		}
	}
	Beep("NAV_UP_DOWN");
}

void MenuSystem::Clear()
{
	m_pages.clear();
	m_open = false;
	m_page = 0;
	m_sel = 0;
	m_scroll = 0;
	m_listening = false;
	m_listenTarget = nullptr;
	m_changed = false;
}

void MenuSystem::Close(){
	m_open = false;
	m_listening = false;
	m_listenTarget = nullptr;
	Beep("BACK");
}

void MenuSystem::ToggleOpen()
{
	if (m_open)
		Close();
	else
		Open(0);
}

bool MenuSystem::ConsumeChanged()
{
	bool c = m_changed;
	m_changed = false;
	return c;
}

int MenuSystem::VisibleRows() const
{
	if (m_page < 0 || m_page >= (int)m_pages.size())
		return 0;
	int n = (int)m_pages[m_page].items.size();
	return n < MAX_ROWS ? n : MAX_ROWS;
}

void MenuSystem::ClampScroll()
{
	MenuPage *p = Current();
	if (!p)
		return;
	int n = (int)p->items.size();
	if (m_sel < 0) m_sel = 0;
	if (m_sel >= n) m_sel = n - 1;
	if (m_sel < m_scroll) m_scroll = m_sel;
	if (m_sel >= m_scroll + MAX_ROWS) m_scroll = m_sel - MAX_ROWS + 1;
	if (m_scroll < 0) m_scroll = 0;
	if (m_scroll > n - MAX_ROWS) m_scroll = n - MAX_ROWS;
	if (m_scroll < 0) m_scroll = 0;
}

void MenuSystem::MoveSelection(int delta)
{
	MenuPage *p = Current();
	if (!p || p->items.empty())
		return;

	int n = (int)p->items.size();
	int idx = m_sel;
	for (int guard = 0; guard < n; guard++)
	{
		idx += delta;
		if (idx >= n) idx = 0;
		if (idx < 0) idx = n - 1;
		if (p->items[idx].Selectable())
			break;
	}

	if (idx != m_sel)
	{
		m_sel = idx;
		m_repH = AxisRepeatState{ 0, 0, 0 }; // do not carry a held left/right into the new row
		Beep("NAV_UP_DOWN");
	}
	ClampScroll();
}

void MenuSystem::Activate()
{
	MenuPage *p = Current();
	if (!p || m_sel < 0 || m_sel >= (int)p->items.size())
		return;

	MenuItem &it = p->items[m_sel];
	switch (it.kind)
	{
	case ItemKind::Toggle:
		if (it.flag) { *it.flag = !*it.flag; m_changed = true; Beep("SELECT"); }
		break;

	case ItemKind::Option:
		if (it.ivalue && !it.options.empty())
		{
			*it.ivalue = (*it.ivalue + 1) % (int)it.options.size();
			m_changed = true;
			Beep("SELECT");
		}
		break;

	case ItemKind::Submenu:
		if (it.submenu >= 0 && it.submenu < (int)m_pages.size())
		{
			m_page = it.submenu;
			m_sel = 0;
			m_scroll = 0;
			m_repV = AxisRepeatState{ 0, 0, 0 };
			m_repH = AxisRepeatState{ 0, 0, 0 };
			MenuPage *np = Current();
			if (np)
			{
				for (size_t i = 0; i < np->items.size(); i++)
					if (np->items[i].Selectable()) { m_sel = (int)i; break; }
			}
			ClampScroll();
			Beep("SELECT");
		}
		break;

	case ItemKind::Action:
		if (it.action) { it.action(); Beep("SELECT"); }
		break;

	case ItemKind::KeyBind:
		if (it.ivalue)
		{
			m_listening = true;
			m_listenTarget = it.ivalue;
			Beep("SELECT");
		}
		break;

	default:
		break;
	}
}

void MenuSystem::GoBack()
{
	if (m_page != 0)
	{
		m_page = 0;
		m_sel = 0;
		m_scroll = 0;
		m_repV = AxisRepeatState{ 0, 0, 0 };
		m_repH = AxisRepeatState{ 0, 0, 0 };
		Beep("BACK");
	}
	else
	{
		Close();
	}
}

// One axis. Triggers on the key-down edge, then keeps firing while the key is
// held, speeding up over time. The scheduler itself lives in axisrepeat.cpp so
// it can be tested without the game.
// Each axis takes a bound key plus a permanent fallback (the arrow keys), so the
// menu stays navigable even if a binding is wrong for the player's keyboard.
int MenuSystem::AxisStep(AxisRepeatState &state, int negKeyA, int negKeyB, int posKeyA, int posKeyB)
{
	const bool posJust = (posKeyA && IsKeyJustDown((DWORD)posKeyA, true)) ||
	                     (posKeyB && IsKeyJustDown((DWORD)posKeyB, true));
	const bool negJust = (negKeyA && IsKeyJustDown((DWORD)negKeyA, true)) ||
	                     (negKeyB && IsKeyJustDown((DWORD)negKeyB, true));

	const bool posHeld = (posKeyA && IsKeyDown((DWORD)posKeyA)) ||
	                     (posKeyB && IsKeyDown((DWORD)posKeyB));
	const bool negHeld = (negKeyA && IsKeyDown((DWORD)negKeyA)) ||
	                     (negKeyB && IsKeyDown((DWORD)negKeyB));

	return AdvanceAxisRepeat(state, posJust, negJust, posHeld, negHeld, GetTickCount());
}

void MenuSystem::Update(const Settings &settings)
{
	if (!m_open)
		return;

	m_keys = makeKeyMap(settings);

	// ---- key capture ----------------------------------------------------
	if (m_listening)
	{
		int k = Keys::PollAnyKeyUp();
		if (k == VK_ESCAPE)
		{
			m_listening = false;
			m_listenTarget = nullptr;
			Log::Line("keybind: capture cancelled");
		}
		else if (k != 0)
		{
			if (m_listenTarget)
			{
				*m_listenTarget = k;
				m_changed = true;
				Log::Line("keybind: bound to %s (0x%02X)", Keys::Name(k).c_str(), k);
			}
			m_listening = false;
			m_listenTarget = nullptr;
		}
		return; // swallow the rest of the input while capturing
	}

	const KeyMap keys = m_keys;

	// ---- navigation -----------------------------------------------------
	int v = AxisStep(m_repV, keys.up, VK_UP, keys.down, VK_DOWN);
	if (v != 0)
		MoveSelection(v);

	// ---- value adjust ---------------------------------------------------
	int h = AxisStep(m_repH, keys.left, VK_LEFT, keys.right, VK_RIGHT);
	if (h != 0)
	{
		MenuPage *p = Current();
		if (p && m_sel >= 0 && m_sel < (int)p->items.size())
		{
			MenuItem &it = p->items[m_sel];
			if (it.kind == ItemKind::Slider && it.value)
			{
				float nv = *it.value + it.vstep * (float)h;
				if (fabsf(nv) < it.vstep * 0.01f && it.vmin <= 0.0f && it.vmax >= 0.0f)
					nv = 0.0f;
				if (nv < it.vmin) nv = it.vmin;
				if (nv > it.vmax) nv = it.vmax;
				if (nv != *it.value) { *it.value = nv; m_changed = true; }
			}
			else if (it.kind == ItemKind::SliderInt && it.ivalue)
			{
				int nv = *it.ivalue + it.istep * h;
				if (nv < (int)it.vmin) nv = (int)it.vmin;
				if (nv > (int)it.vmax) nv = (int)it.vmax;
				if (nv != *it.ivalue) { *it.ivalue = nv; m_changed = true; }
			}
			else if (it.kind == ItemKind::Option && it.ivalue && !it.options.empty())
			{
				int n = (int)it.options.size();
				*it.ivalue = (*it.ivalue + h + n) % n;
				m_changed = true;
			}
			else if (it.kind == ItemKind::Toggle && it.flag)
			{
				*it.flag = !*it.flag;
				m_changed = true;
			}
		}
	}

	const bool selectPressed = (keys.select && IsKeyJustDown((DWORD)keys.select, true)) ||
	                           IsKeyJustDown(VK_RETURN, true);
	if (selectPressed)
		Activate();

	const bool backPressed = (keys.back && IsKeyJustDown((DWORD)keys.back, true)) ||
	                         IsKeyJustDown(VK_BACK, true);
	if (backPressed)
		GoBack();
}

void MenuSystem::Draw(const Settings &settings)
{
	if (!m_open)
		return;

	MenuPage *p = Current();
	if (!p)
		return;

	const Theme th = BuildTheme(settings);

	const int rows = VisibleRows();
	const float rowsTop = PANEL_Y + HEADER_H + PAGEBAR_H;
	const float rowsH = rows * ROW_H;
	const float panelH = HEADER_H + PAGEBAR_H + rowsH;
	const float footerTop = PANEL_Y + panelH;

	// ---- panel background ----------------------------------------------
	Ui::FillRect(PANEL_X, PANEL_Y, PANEL_W, panelH, COL_PANEL_BG);

	// ---- header (accent bar, auto contrast text) -----------------------
	Ui::FillRect(PANEL_X, PANEL_Y, PANEL_W, HEADER_H, th.accent);
	Ui::TextLeft("CAM CONTROL", PANEL_X + 0.010f, PANEL_Y + 0.008f, 0.45f, th.onAccent, 1);
	if (!m_version.empty())
		Ui::TextRight(m_version, PANEL_X + PANEL_W - 0.010f, PANEL_Y + 0.012f, 0.24f, th.dimOnAccent, 0);
	Ui::TextRight(Lang::Tr("made by sensin"), PANEL_X + PANEL_W - 0.010f, PANEL_Y + 0.036f, 0.23f, th.dimOnAccent, 0);
	Ui::TextRight("akasensin on github", PANEL_X + PANEL_W - 0.010f, PANEL_Y + 0.056f, 0.21f, th.dimOnAccent, 0);

	// ---- page title bar -------------------------------------------------
	{
		Ui::FillRect(PANEL_X, PANEL_Y + HEADER_H, PANEL_W, PAGEBAR_H, COL_HINT_BG);
		std::string title = ">> " + p->title;
		Ui::TextLeft(title, PANEL_X + 0.010f, PANEL_Y + HEADER_H + 0.004f, 0.30f, COL_TEXT, 1);
	}

	// ---- rows -----------------------------------------------------------
	for (int i = 0; i < rows; i++)
	{
		const int idx = m_scroll + i;
		if (idx >= (int)p->items.size())
			break;

		MenuItem &it = p->items[idx];
		const bool active = (idx == m_sel);
		const float y = rowsTop + i * ROW_H;
		const float x = PANEL_X + 0.004f;
		const float w = PANEL_W - 0.008f;

		// Dynamic labels are re-evaluated every frame so they can show live state.
		const std::string rowText = it.textFn ? it.textFn() : it.text;

		if (it.kind == ItemKind::Separator)
		{
			Ui::FillRect(x + 0.006f, y + ROW_H * 0.5f, w - 0.012f, 0.0018f, COL_TRACK);
			continue;
		}

		if (it.kind == ItemKind::Label)
		{
			Ui::TextLeft(rowText, x + 0.008f, y + 0.009f, 0.255f, COL_TEXT_DIM, 0);
			continue;
		}

		Ui::FillRect(x, y + 0.002f, w, ROW_H - 0.004f, active ? th.accent : COL_ROW_BG);

		const int *textCol = active ? th.onAccent : COL_TEXT;
		const int *trackCol = active ? COL_TRACK_ACTIVE : COL_TRACK;
		const int *fillCol = active ? th.onAccent : th.accentSoft;
		const float rightEdge = x + w - 0.008f;

		if ((it.kind == ItemKind::Slider && it.value) || (it.kind == ItemKind::SliderInt && it.ivalue))
		{
			Ui::TextLeft(rowText, x + 0.008f, y + 0.003f, 0.31f, textCol, 0);

			std::string val;
			float norm = 0.0f; // 0..1 position of the fill
			const float range = it.vmax - it.vmin;
			if (it.kind == ItemKind::SliderInt)
				val = FormatNumber((float)*it.ivalue, 1.0f);
			else
			{
				val = FormatNumber(*it.value, it.vstep);
				if (!it.unit.empty())
					val += it.unit;
			}

			const float cur = it.intSlider ? (float)*it.ivalue : *it.value;
			if (range > 0.0001f)
				norm = (cur - it.vmin) / range;
			if (norm < 0.0f) norm = 0.0f;
			if (norm > 1.0f) norm = 1.0f;

			Ui::TextRight(val, rightEdge, y + 0.003f, 0.31f, textCol, 0);

			const float bx = x + 0.008f;
			const float bw = w - 0.016f;
			const float by = y + ROW_H - 0.0125f;
			Ui::FillRect(bx, by, bw, 0.0065f, trackCol);
			Ui::FillRect(bx, by, bw * norm, 0.0065f, fillCol);

			// thumb
			Ui::FillRect(bx + bw * norm - 0.0012f, by - 0.0022f, 0.0024f, 0.0110f, fillCol);
		}
		else
		{
			Ui::TextLeft(rowText, x + 0.008f, y + 0.0085f, 0.31f, textCol, 0);

			std::string right;
			if (it.kind == ItemKind::Toggle && it.flag)
				right = *it.flag ? Lang::Tr("ON") : Lang::Tr("OFF");
			else if (it.kind == ItemKind::Option && it.ivalue && !it.options.empty())
			{
				int oi = *it.ivalue;
				if (oi < 0) oi = 0;
				if (oi >= (int)it.options.size()) oi = (int)it.options.size() - 1;
				right = it.options[oi];
			}
			else if (it.kind == ItemKind::Submenu)
				right = ">";
			else if (it.kind == ItemKind::KeyBind && it.ivalue)
			{
				if (m_listening && it.ivalue == m_listenTarget)
					right = Lang::Tr("[ PRESS KEY ]");
				else
					right = Keys::Name(*it.ivalue);
			}

			if (!right.empty())
			{
				// Lang::Tr("ON") gets an extra visual cue so toggles read at a glance
				if (it.kind == ItemKind::Toggle && it.flag && *it.flag)
					Ui::TextRight(right, rightEdge, y + 0.0085f, 0.31f, textCol, 1);
				else
					Ui::TextRight(right, rightEdge, y + 0.0085f, 0.29f, textCol, 0);
			}
		}
	}

	// ---- scrollbar ------------------------------------------------------
	{
		const int total = (int)p->items.size();
		if (total > MAX_ROWS)
		{
			const float sx = PANEL_X + PANEL_W - 0.004f;
			Ui::FillRect(sx, rowsTop, 0.0022f, rowsH, COL_TRACK);
			float thumbH = rowsH * ((float)MAX_ROWS / (float)total);
			float maxScroll = (float)(total - MAX_ROWS);
			float t = (maxScroll > 0.0f) ? ((float)m_scroll / maxScroll) : 0.0f;
			Ui::FillRect(sx, rowsTop + (rowsH - thumbH) * t, 0.0022f, thumbH, th.accentSoft);
		}
	}

	// ---- footer ---------------------------------------------------------
	{
		Ui::FillRect(PANEL_X, footerTop, PANEL_W, FOOTER_H, COL_HINT_BG);

		if (m_listening)
		{
			Ui::TextLeft(Lang::Tr("Press any key to bind  -  ESC to cancel"),
				PANEL_X + 0.010f, footerTop + 0.006f, 0.26f, COL_TEXT, 1);
		}
		else
		{
			// Two short lines. The single-line hint was wider than the panel, and
			// the right-aligned credit was drawn straight over it - that is the
			// white text that sat on top of the grey hint. The credit already
			// appears in the header, so it is not repeated here.
			char l1[160];
			char l2[160];
			_snprintf_s(l1, sizeof(l1), _TRUNCATE,
				Lang::Tr("%s/%s move    %s/%s change"),
				Keys::Name(m_keys.up).c_str(), Keys::Name(m_keys.down).c_str(),
				Keys::Name(m_keys.left).c_str(), Keys::Name(m_keys.right).c_str());
			_snprintf_s(l2, sizeof(l2), _TRUNCATE,
				Lang::Tr("%s select    %s back"),
				Keys::Name(m_keys.select).c_str(), Keys::Name(m_keys.back).c_str());
			Ui::TextLeft(l1, PANEL_X + 0.010f, footerTop + 0.002f, 0.215f, COL_TEXT_DIM, 0);
			Ui::TextLeft(l2, PANEL_X + 0.010f, footerTop + 0.015f, 0.215f, COL_TEXT_DIM, 0);
		}
	}
}
