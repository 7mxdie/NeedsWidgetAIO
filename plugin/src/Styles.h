#pragma once

// 3.0 icon styles (Tom, 2026-11-23: "LOADS of choice"). Each style is an ini file in
// Data/SKSE/Plugins/NeedsWidget/Styles/ with a frame picture in Interface/NeedsWidget/icons/.
// The shipped ones are made by swf/styles/build_styles.py; the format is in docs/STYLES.md,
// a contract with anyone who makes their own: add keys, never change what one means.
// Ring, Bar, Icon only and Fill stay the built-in looks (General::look); a style is picked
// by its file name (General::style, sStyle) and wins while its file is there.
namespace Styles
{
	struct Style
	{
		std::string key;    // the file name without .ini: what sStyle saves
		std::string name;   // UTF-8, for the menu
		std::string note;
		std::string frame;  // picture in icons/, empty for none
		std::string meter;  // ring, trace, bar, steps, level, glyph or none
		float       width{ 2.0f };
		std::string geometry;
		std::string by;
		int         alpha{ 55 };
		float       artSize{ 40.0f };
		float       artY{ 0.0f };
		std::int64_t glyph{ -1 };  // 0xRRGGBB, or -1 for the stage colour
		std::array<std::uint32_t, 6> colors{ 0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F };
		bool        ownColors{ false };
		bool        tintFrame{ false };
		float       top{ -30.0f };     // the style's extent, icon units: where the numbers go
		float       bottom{ 30.0f };
		// sDetect (3.0): files under Data/ that show the UI mod this style suits is installed, split by |.
		// The first style whose file is there is offered once on the General page.
		std::vector<std::string> detect;
	};

	void                       Scan();  // reads the Styles folder (again)
	std::vector<Style>         List();  // A to Z by name
	std::optional<Style>       Find(std::string_view a_key);
	std::optional<Style>       Detected();  // the first style (A to Z) whose sDetect file is in Data/, if any
}
