/*
	Cam Control - made by sensin
	Small self contained game menu: themable accent colour, sliders, key binding.

	Pages are built once at startup from script.cpp with the Add* helpers.

	Text handed to the Add* helpers must already be translated by the caller
	(Lang::Tr). The one exception is Option(), whose option list is a set of
	English keys that the menu translates itself.
*/

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "axisrepeat.h"
#include "settings.h"

enum class ItemKind
{
	Toggle,
	Slider,
	SliderInt,
	Option,
	Submenu,
	Action,
	KeyBind,
	Label,
	Separator
};

struct MenuItem
{
	ItemKind kind = ItemKind::Action;
	std::string text;
	std::function<std::string()> textFn; // when set, supplies the text every frame
	bool *flag = nullptr;
	float *value = nullptr;
	int *ivalue = nullptr;
	float vmin = 0.0f;
	float vmax = 1.0f;
	float vstep = 0.1f;
	int istep = 1;
	bool intSlider = false;
	std::string unit;
	std::vector<std::string> options;
	int submenu = -1;
	std::function<void()> action;

	bool Selectable() const { return kind != ItemKind::Label && kind != ItemKind::Separator; }
};

struct MenuPage
{
	std::string title;
	std::vector<MenuItem> items;
};

class MenuSystem
{
public:
	MenuSystem();

	// ---- building -------------------------------------------------------
	int  AddPage(const std::string &title);
	void Toggle(int page, const char *text, bool *flag);
	void Slider(int page, const char *text, float *value, float vmin, float vmax, float step, const char *unit = nullptr);
	// Integer slider (used by the theme R/G/B channels).
	void SliderInt(int page, const char *text, int *value, int vmin, int vmax, int step);
	void Option(int page, const char *text, int *value, const std::vector<std::string> &options);
	void Submenu(int page, const char *text, int targetPage);
	void Action(int page, const char *text, std::function<void()> action);
	void KeyBind(int page, const char *text, int *vk);
	void Label(int page, const char *text);
	// Label whose text is produced fresh every frame (for live status readouts).
	void Label(int page, std::function<std::string()> textFn);
	void Separator(int page);

	// ---- state ----------------------------------------------------------
	bool IsOpen() const { return m_open; }
	bool IsListening() const { return m_listening; }
	// Drops every page. Used to rebuild the menu in another language, since the
	// text of each row is resolved when the page is built.
	void Clear();
	void Open(int page = 0);
	void Close();
	void ToggleOpen();

	// Set once per frame when the player changed something. Cleared by the caller.
	bool ConsumeChanged();

	// ---- per frame ------------------------------------------------------
	void Update(const Settings &settings);
	void Draw(const Settings &settings);

private:
	std::vector<MenuPage> m_pages;
	bool m_open = false;
	int m_page = 0;
	int m_sel = 0;
	int m_scroll = 0;
	bool m_changed = false;

	// key bind capture
	bool m_listening = false;
	int *m_listenTarget = nullptr;

	// Held-key repeat state (scheduler lives in axisrepeat.cpp).
	AxisRepeatState m_repV{ 0, 0, 0 };
	AxisRepeatState m_repH{ 0, 0, 0 };

	// last keymap seen, used for the footer hints
	KeyMap m_keys{ 0x26, 0x28, 0x25, 0x27, 0x0D, 0x08 };
	std::string m_version;

public:
	void SetVersion(const std::string &v) { m_version = v; }

private:
	MenuPage *Current();
	int VisibleRows() const;
	void MoveSelection(int delta);
	void Activate();
	void GoBack();
	void ClampScroll();
	int AxisStep(AxisRepeatState &state, int negKeyA, int negKeyB, int posKeyA, int posKeyB);
};
