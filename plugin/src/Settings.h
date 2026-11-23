#pragma once

#include "Needs.h"

// Layout and look. Positions are percentages of the visible screen.
//
// Two files, so a reskin or a modlist can ship its own defaults without ever
// touching what the player has saved:
//   Data/SKSE/Plugins/NeedsWidget.ini         - shipped defaults and style, never written
//   Data/SKSE/Plugins/NeedsWidget_Custom.ini  - what the player saved
// Each icon has its own section ([Hunger], [Cold] ...); the look is in [General].
namespace Settings
{
	struct Icon
	{
		float x{ 88.0f };
		float y{ 87.0f };
		float size{ 75.0f };
		float opacity{ 100.0f };
		bool  enabled{ true };
		bool  showNumber{ false };
		bool  hideWhenFine{ false };
		bool  hideFromStage{ false };  // only for an extra icon with iHideFrom: hide at that stage and worse
		// This icon's number moved from where General puts it, in icon units (2.9.0, phbd01:
		// stage names for a vertical HUD). fNumberX / fNumberY.
		float numberX{ 0.0f };
		float numberY{ 0.0f };
		// sSource (2.9.0, Brunny): read only from this mod (a source's sMod / mod name), for
		// a game with two mods for one need. Empty: the first one running, as before.
		std::string source;
		// iFineUpTo (2.10, HardSignage): Hide when fine hides stages up to this one; -1 is the
		// icon's own (Needs::DefaultFineUpTo).
		int fineUpTo{ -1 };
		// sArt (3.0, Tom): the picture this icon shows, a file name in Interface/NeedsWidget/icons/
		// without .swf; empty is the icon's own.
		std::string art;
		// 3.0 (Fable's ideas, Tom: "add them all"). iWhere: 0 anywhere, 1 only outdoors, 2 only
		// indoors. iGroup: 0 the main row or column, 1 the second one (General::line2X/Y).
		int where{ 0 };
		int group{ 0 };

		bool operator==(const Icon&) const = default;
	};

	enum class Look : int
	{
		kRing,  // a ring round the glyph fills as the need gets worse
		kBar,   // a small bar under the glyph
		kIcon,  // the glyph alone, coloured by stage
		kFill   // 2.9.0 (AspardMG): the inside of the ring fills from the bottom, like a level
	};

	// Which colours the stages use. The schemes are fixed; Custom uses
	// General::customColors, which the player picks on the General page and a
	// reskin can set in the defaults file.
	enum class ColorScheme : int
	{
		kBoneToRed,   // bone white when fine, then gold, orange, red, bright red (Tom, 2026-10-21)
		kGreenToRed,  // traffic light: green when fine through to bright red
		kCustom,
		kStyle,  // 3.0: the picked style's own six colours (Styles::Style::colors); bone to red for Ring, Bar, Icon only and Fill
		kBlueToOrange  // 3.0: for colour-blind players, pale blue to orange (Okabe-Ito); pair with bStagePips
	};

	// How the icons are laid out (Tom, TEST4: "make them vertical instead of
	// horizontal, reorder them").
	enum class Arrangement : int
	{
		kRow,     // side by side, in `order`, from one end of the row
		kColumn,  // one above another, in `order`, from one end of the column
		kFree     // each icon at its own X and Y, as before
	};

	// Left to right, or top to bottom. Thirst sits by hunger, wetness by cold.
	// Disease is hidden while healthy, so it sits at the free end: when it turns up
	// the others don't move (Tom, 2026-10-21: it threw the Vel'dun row out). Extra
	// icons from addons go before it, out at the free end as well.
	constexpr std::array<int, Needs::kCount> DefaultOrder()
	{
		std::array<int, Needs::kCount> order{};
		std::size_t                    n = 0;
		for (int id = Needs::kBuiltInCount; id < Needs::kCount; ++id) {
			order[n++] = id;
		}
		for (const int id : { Needs::kDisease, Needs::kHunger, Needs::kThirst, Needs::kCold, Needs::kWetness, Needs::kFatigue,
				 Needs::kVitality, Needs::kStress, Needs::kDirt, Needs::kEncumbrance }) {
			order[n++] = id;
		}
		return order;
	}

	struct General
	{
		// The whole widget on or off (bShowWidget). Off hides every icon and changes nothing else.
		// A preset never changes it.
		bool        showWidget{ true };
		// One icon per disease (3.0, GenieA; Tom: a choice, not a change): Disease shows the worst,
		// the Disease 2-6 icons (Icons/Disease N.ini, sType DiseaseRank) the next ones. Off: they
		// don't exist anywhere, and Disease names the worst with (+N). bOneIconPerDisease.
		bool        diseaseEach{ false };
		// The menu offered the style made for the UI mod it found (3.0), once: used or turned down,
		// it never asks again (bStyleOffered). Presets never change it.
		bool        styleOffered{ false };
		Look        look{ Look::kRing };
		// 3.0 icon styles (Tom, 2026-11-23): a style's file name in NeedsWidget/Styles/ (sStyle, "-" for
		// none). While it names a style that is there, it wins over look; empty means look.
		std::string style;
		bool        tintArt{ true };  // colour the glyphs by stage too, not only the meter
		ColorScheme colors{ ColorScheme::kStyle };
		bool        pulseWorst{ true };  // fade an icon in and out while its need is at its worst stage
		// The numbers (Tom, TEST7/8): over or under the icon, their own size (they
		// scale with the icon too), how far from it and sideways, in icon units.
		bool        numbersAbove{ false };
		float       numberSize{ 100.0f };
		float       numberGap{ 0.0f };
		float       numberX{ 0.0f };
		bool        numberTint{ false };  // colour the number like its icon
		bool        numberEdge{ true };   // a dark edge round the number (phbd01, 2.3.0)
		int         numberShows{ 0 };     // 0 the percent, 1 the stage's name (TEST10), 2 game hours until the worst stage (3.0)
		int         numberAlign{ 0 };     // 0 centred under the icon, 1 starts at it, 2 ends at it (phbd01, vertical HUDs)
		// HjalmarSeavuulf, 2026-10-27 (sight): the numbers' font, one of the game's own
		// (fontconfig.txt names, so a font mod still applies), and their letter case.
		int         numberFont{ 0 };      // 0 medium (as before), 1 bold, 2 light, 3 plain (the console's), 4 book, 5 handwritten
		int         numberCase{ 0 };      // 0 as written, 1 lower case, 2 upper case
		// Aupronoch, 2026-10-27: the meter shows what's left and drains as a need gets
		// worse, instead of filling. The percent follows it.
		bool        meterDrains{ false };

		// TEST10 (Tom: "All but 5"). Fade the widget out once nothing has changed for
		// fadeAfter seconds, back in on any change; keepBad keeps it up while a need
		// is at its worst two stages. flashWorse flashes an icon when its stage rises.
		bool  fadeWhenStill{ false };
		float fadeAfter{ 8.0f };
		bool  keepBad{ true };
		bool  flashWorse{ true };

		// The widget's key (a keyboard scan code, 0 = none). 0: it hides and shows
		// the widget (flips showWidget, kept at once); 1: it brings a faded widget back for a while.
		int hotkey{ 0 };
		int hotkeyMode{ 0 };  // 2 (3.0): while held, every icon shows with its stage and what it does to you

		// All icons together (Tom, 2026-10-21: scale them all at once, not one by one).
		// groupSize scales every icon and the gaps between them about the middle of
		// the default layout; groupX and groupY then move the lot, in percent of the
		// screen. Each icon's own place and size still apply inside the group.
		float groupSize{ 100.0f };
		float groupX{ 0.0f };   // Free only: the row and column have their own position
		float groupY{ 0.0f };

		// Row and Column. The position is the centre of the icon at the chosen end
		// (the right end of a row, the bottom of a column, by default), so the line
		// grows away from it and closing gaps pulls the icons towards it.
		Arrangement               arrangement{ Arrangement::kRow };
		bool                      fromFarEnd{ true };  // right end of a row, bottom end of a column
		float                     lineX{ 96.0f };
		float                     lineY{ 87.0f };
		float                     spacing{ 7.0f };     // gap between icons, in percent of an icon's size
		bool                      closeGaps{ true };   // a hidden icon leaves no hole
		// A long row or column wraps onto another (starchaser0392, 2026-10-23: "add row
		// options, cuz it's going to be too damn long"). 0 keeps it on one line.
		int                       perLine{ 0 };
		bool                      wrapBefore{ true };  // new rows go above a row, new columns left of a column
		float                     lineGap{ 15.0f };    // between rows or columns, in percent of an icon's size
		std::array<int, Needs::kCount> order{ DefaultOrder() };

		// The Custom colour scheme, best (0) to worst (5), as 0xRRGGBB (Tom, TEST5:
		// pick them in the menu, not only in the ini). Starts as bone to red.
		std::array<std::uint32_t, 6> customColors{ 0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F };

		// 3.0, Fable 5.2's ideas (Tom: "add them all. 3.0 is meant to be the best of the best").
		bool  worstFirst{ false };    // Row/Column in order of how bad each need is, worst at the chosen end (bWorstFirst)
		int   maxShown{ 0 };          // show only this many, the worst; 0 = all (iMaxShown)
		float growBad{ 0.0f };        // icons at stage 4 and 5 grow by this many percent (fGrowBad)
		bool  hideInCombat{ false };  // bHideInCombat
		bool  hideWeaponDrawn{ false };  // bHideWeaponDrawn
		bool  hideMounted{ false };   // bHideMounted
		bool  showInMenus{ false };   // stay up, every icon shown, in Sleep/Wait, inventory, barter, containers, gifts (bShowInMenus)
		bool  flashBetter{ false };   // a soft flash when a stage gets better (bFlashBetter)
		bool  callout{ false };       // the new stage's name shows by the icon for a few seconds (bStageCallout)
		int   soundFrom{ 0 };         // play a sound when a need reaches this stage or worse; 0 = never (iSoundFrom)
		int   sound{ 0 };             // which: 0 soft, 1 alert, 2 quest (iSound)
		bool  stagePips{ false };     // five small pips by each icon, one lit per stage (bStagePips)
		bool  trend{ false };         // a small arrow: getting worse or better (bTrendArrow)
		// The second row or column (iGroup 1 icons), placed like the first (fLine2PosX/Y).
		float line2X{ 4.0f };
		float line2Y{ 87.0f };

		bool operator==(const General&) const = default;
	};

	// What a reskin sets that has no slider: picture names and where the pictures
	// and the meter sit, in icon units (the icon is 64 across, centred on 0,0).
	// Read from the defaults file only.
	struct Style
	{
		std::string ringFrame{ "frame_ring" };
		float       ringArtSize{ 42.0f };
		float       ringArtY{ 0.0f };
		float       ringRadius{ 27.5f };
		float       ringWidth{ 1.8f };
		float       ringLabelY{ 31.0f };

		std::string barFrame{ "frame_bar" };
		float       barArtSize{ 44.0f };
		float       barArtY{ -4.0f };
		float       barLeft{ -16.2f };
		float       barTop{ 21.8f };
		float       barLength{ 32.4f };
		float       barHeight{ 0.8f };
		float       barLabelY{ 25.0f };

		float iconArtSize{ 46.0f };
		float iconArtY{ 0.0f };
		float iconLabelY{ 22.0f };
	};

	// A whole look and layout in one file (Tom, 2026-10-21: presets in the menu, not
	// separate patches; players on other UIs send theirs in). Presets are ini files
	// in Data/SKSE/Plugins/NeedsWidget/Presets/, written like NeedsWidget_Custom.ini
	// with a [Preset] section for the name and a line about it. Anything a preset
	// leaves out comes from the defaults, so applying one always gives the same result.
	struct Preset
	{
		std::string                     name;  // UTF-8, for the menu
		std::string                     note;
		std::filesystem::path           file;
		General                         general;
		std::array<Icon, Needs::kCount> icons;
	};

	void    Load();
	bool    Save();                           // writes only what differs from the defaults; false if the file couldn't be written
	bool    SaveShown(Needs::Id a_id);        // keeps that icon's Show this icon now, and leaves every other unsaved change unsaved
	bool    SaveShowWidget(bool a_show);      // sets Show the widget and keeps it now, the same way (the hotkey and the General page)
	bool    SaveStyleOffered();               // the UI-mod style offer was answered: kept now, the same way
	void    ScanPresets();                    // reads the Presets folder again
	std::vector<Preset> GetPresets();
	void    ApplyPreset(const Preset& a_preset);  // like moving the sliders: Save makes it stick
	bool    IsInUse(const Preset& a_preset);
	bool    MatchesDefaults();
	std::filesystem::path ExportPreset();     // every setting, to a new file; empty on failure
	std::string PresetText();                 // every setting as preset text, for the clipboard (3.0)
	std::filesystem::path PastePreset(const std::string& a_text);  // saves pasted preset text as a new preset and applies it; empty if it isn't one
	void    RestoreAllDefaults();
	void    ShowAllNumbers(bool a_show);       // every icon's "Show the number" at once
	void    RestoreDefaults(Needs::Id a_id);  // that icon only; Save makes it stick
	void    RestoreGeneralDefaults();
	Icon    Get(Needs::Id a_id);
	void    Set(Needs::Id a_id, const Icon& a_icon);
	General GetGeneral();
	General GetGeneralDefaults();             // what Restore General's defaults puts back
	Icon    GetDefaults(Needs::Id a_id);      // what Restore this icon's defaults puts back
	void    SetGeneral(const General& a_general);
	Style   GetStyle();
	std::array<std::uint32_t, 6> StageColors(ColorScheme a_scheme);  // best to worst; Custom gives the current custom colours
	std::array<std::uint32_t, 6> StageColors(const General& a_general);
	bool    HasUnsavedChanges();

	// True once after every change, so the HUD layer applies it on its next frame.
	bool ConsumeDirty();

	// Where an icon really goes once the group settings are applied.
	struct Placed
	{
		float x;
		float y;
		float size;
	};
	Placed Place(const Icon& a_icon, const General& a_general);

	// Our settings pages call MarkEditing every time they draw, which SKSE Menu
	// Framework only does while one of them is on screen. While that is recent,
	// the HUD layer shows hidden icons faded, so they can be placed.
	void MarkEditing();
	bool IsEditing();
}
