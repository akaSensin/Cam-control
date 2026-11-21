/*
	Cam Control - made by sensin

	Localization.

	The English text is the key: Tr("Field Of View") looks the string up and
	returns the current language's version. Nothing is ever looked up by a
	numeric id, so a string can be added, moved or reworded without touching a
	table index, and a missing translation falls back to English instead of
	showing garbage.

	Not every language can be drawn by GTA's own text engine:

	  Latin + Cyrillic (English, Turkish, Russian) - native path.
	  Arabic  - needs right-to-left ordering AND contextual glyph shaping.
	  CJK     - the game font has no Chinese/Japanese glyphs at all.
	  Spanish, German, Portuguese - Latin with accents (a-tilde, u-umlaut,
	            c-cedilla). The game drew some of those blank, so they take the
	            overlay too rather than depend on which glyphs its font carries.
	              Indonesian needs no accents at all, so it stays on the native
	              path with English/Turkish/Russian.

	The last two are therefore drawn by our own DirectWrite overlay (dxtext.*),
	which is also where the font for them comes from. NeedsCustomRender() says
	which path a language takes.

	Translations can be corrected WITHOUT rebuilding: put a file named
	lang_<code>.txt next to the .asi with "English text=translated text" lines
	and any of them replace the built-in ones.
*/

#pragma once

#include <string>

namespace Lang
{
	enum Id
	{
		English = 0,
		Turkish,
		Spanish,
		Russian,
		Arabic,
		Chinese,
		Japanese,
		// Appended, never inserted: the numbers are what CamControl.ini stores,
		// so inserting would silently change an existing user's language.
		German,
		Portuguese,
		Indonesian,
		COUNT
	};

	// Two letter code used for the config, the override file name and the log.
	const char *Code(Id id);

	// Native name, shown in the menu ("Türkçe", "العربية", "中文", ...).
	const char *DisplayName(Id id);

	void Init(const std::wstring &baseDir);
	void Set(Id id);
	Id   Current();

	// True when the language is right-to-left.
	bool IsRtl(Id id);

	// True when GTA's own text engine cannot draw this language.
	bool NeedsCustomRender(Id id);
	bool NeedsCustomRender();

	// Windows font family used by the DirectWrite overlay for this language.
	const wchar_t *DxFontFamily(Id id);

	// Looks up a string. Returns the key itself when the current language is
	// English or the translation is missing.
	const char *Tr(const char *key);

	// UTF-8 -> UTF-16, for the DirectWrite path.
	std::wstring Widen(const char *utf8);

	std::string CurrentCode();
}
