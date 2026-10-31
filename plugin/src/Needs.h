#pragma once

// Reads what the icons show, straight from the game. Each need can come from
// several mods; the first one in the list that is running feeds the icon
// (TEST7, Tom: "coverage for every need you find in modded Skyrim"):
//   hunger      - Survival Mode (+ Survival Mode Improved), SunHelm, Last Seed, iNeed
//   cold        - Survival Mode (+ SMI), SunHelm, Frostfall
//   fatigue     - Survival Mode (+ SMI), SunHelm, Last Seed, iNeed
//   thirst      - SunHelm, Last Seed, iNeed, DVSMP Survival Tweaks (LoreRim's thirst), Tears of Kyne
//   wetness     - Frostfall
//   stress      - Stress and Fear
//   dirt        - Dirt and Blood, Bathing in Skyrim, Keep It Clean
//   encumbrance - the player's inventory weight against carry weight
//   vitality    - Last Seed's overall condition (TEST10)
//   disease     - any Disease spell on the player; Survival Mode, Last Seed and iNeed say how bad
// Every record was read from the mod's own plugin and scripts; see
// docs/NEEDS-MODS.md. Forms are looked up once after data has loaded. A missing
// mod just leaves its icon hidden.
//
// After the built-in needs come slots for extra icons, which addon mods describe
// in ini files (Extras.h, docs/EXTRA-ICONS.md). A slot no file fills is never shown.
namespace Needs
{
	// The numbers are the ini's sOrder values, so new needs go on the end of the
	// built-in ones. Extra icons are saved by name, never by slot number.
	enum Id : std::uint32_t
	{
		kHunger,
		kCold,
		kFatigue,
		kStress,
		kDirt,
		kEncumbrance,
		kThirst,
		kWetness,
		kVitality,
		kDisease,
		kBuiltInCount,
		kCount = kBuiltInCount + 64  // widget.as makes this many icons too (ICON_COUNT)
	};

	constexpr bool IsExtra(Id a_id) { return a_id >= kBuiltInCount && a_id < kCount; }

	struct Reading
	{
		bool        available{ false };  // a mod behind it is installed and switched on
		float       value{ 0.0f };
		float       max{ 0.0f };
		float       fraction{ 0.0f };  // value / max, not clamped (encumbrance can pass 1)
		int         stage{ 0 };        // 0 = best, 5 = worst
		const char* stageName{ nullptr };  // a source can name the stage itself; else its names[stage]
		const char* source{ "" };      // the mod it was read from
		bool        showValue{ false };  // bShowValue: the number is the value itself, not a percent
	};

	void Init();

	// Whether the slot holds an icon at all: every built-in need, and an extra slot
	// once an addon file has filled it (Extras::Scan, at kPostLoad).
	bool Used(Id a_id);

	// Call on the game thread. Also keeps the result for Last().
	Reading Read(Id a_id);
	// The last reading taken on the game thread, for the settings page.
	Reading Last(Id a_id);

	// Whether any mod behind this need is in the game at all, switched on or not.
	// Known once Init has run. Needs whose mods are all missing get no settings
	// page and no place in a row or column (Tom, TEST6: no page "taking up space
	// for no reason").
	bool        Installed(Id a_id);
	const char* Name(Id a_id);         // "Hunger" ..., also its ini section and page name; "" for an empty slot
	std::string WhyHidden(Id a_id);    // why it is unavailable, for the settings page
	std::string FineMeaning(Id a_id);  // what "Hide when fine" counts as fine, in the current mod's words
	int         FineUpTo(Id a_id);     // "Hide when fine" hides stages up to this one
	int         HideFrom(Id a_id);     // iHideFrom: the stage its own checkbox hides from, -1 if it has none
	std::string HideFromText(Id a_id);
}
