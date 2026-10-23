#pragma once

#include "Needs.h"

#include <optional>

// Extra icons that addon mods add without any code: one ini file each in
// Data/SKSE/Plugins/NeedsWidget/Icons/, describing what to read and how to stage
// it. The format is a contract with addons in the wild (docs/EXTRA-ICONS.md): new
// keys and source types can be added, existing ones never change meaning.
//
// Scan runs at kPostLoad, before the settings are loaded, so each extra icon's
// name (its ini section) is known when they are. Needs::Init turns the sources
// into readers once the game's data has loaded.
namespace Extras
{
	// One [SourceN] section.
	struct SourceDef
	{
		std::string              mod;      // sMod, for the menu and the log
		std::string              type;     // sType, lower case: global, factionrank, actorvalue, papyrus, ...
		std::vector<std::string> plugins;  // sPlugin, split on |: any of them loaded counts
		std::uint32_t            formID{ 0 };  // iFormID, local to the plugin
		std::string              editorID;     // sEditorID (globals)
		std::string              actorValue;   // sActorValue
		std::string              script;       // sScript
		std::string              function;     // sFunction
		std::string              key;          // sKey (StorageFloat)
		std::string              variable;     // sVariable (ScriptVariable; split on , the highest counts)
		int                      alias{ -1 };  // iAlias (ScriptVariable): the quest's alias with this ID, -1 the quest itself, -2 the alias the player fills
		std::vector<std::pair<std::uint32_t, float>> spells;  // sSpells (Spells): local form ID : value
		std::vector<std::pair<std::uint32_t, float>> wornKeywords;  // sWornKeywords (Spells): keywords on what the player wears
		std::vector<std::pair<std::uint32_t, float>> effects;       // sEffects (MagicEffect): magic effects on the player
		std::vector<std::string> keys;        // sKey split on , (StorageFloat): the values add up
		bool                     noActor{ false };  // bNoActor (StorageFloat): stored on None, not the player
		bool                     passPlayer{ true };  // bPassPlayer
		bool                     onPlayer{ false };   // bOnPlayer: call sFunction on the player, as an sScript
		std::vector<std::string> args;                // sArgs (Papyrus, QuestFunction): one call per text, after the player; the answers add up
		std::string              maxFunction;         // sMaxFunction (Papyrus, QuestFunction): the reading becomes 100 x answer / its answer
		std::string              maxEditorID;         // sMaxEditorID (Global): the reading becomes 100 x value / that global
		std::uint32_t            maxFormID{ 0 };      // iMaxFormID (Global)
		bool                     negativeRanks{ false };  // bNegativeRanks (FactionRank): ranks under 0 count, but -1 and -2 (not in it)
		float                    every{ 1.0f };       // fEvery, seconds between Papyrus calls
		std::optional<float>     noValueBelow;        // fNoValueBelow: a reading under this means the mod isn't tracking it
		struct Remap
		{
			float from, to, value;
		};
		std::vector<Remap> remap;  // sRemap = <from>-<to>:<value>, ...: a reading in a range becomes that value

		// An on/off global in the mod (sSwitchEditorID and/or iSwitchFormID, in sPlugin):
		// the source only counts while it equals fSwitchOn. Any type.
		std::string   switchEditorID;
		std::uint32_t switchFormID{ 0 };
		float         switchOn{ 1.0f };

		// Resolved against [Icon]'s when the section leaves them out.
		std::array<std::string, 6> stages;  // sStage0-5, for a mod whose stages mean something else
		float                max{ 100.0f };
		bool                 higherIsWorse{ true };
		std::array<float, 5> steps{};
	};

	struct Def
	{
		std::filesystem::path      file;
		std::string                name;  // sName: the menu, the log, its ini section
		std::string                art;   // sArt: icons/<art>.swf
		std::string                page;  // sPage, empty for the Icons page
		std::array<std::string, 6> stages;
		int                        fineUpTo{ 1 };
		std::string                fine;  // sFine, empty to build it from the stage names
		std::vector<SourceDef>     sources;
	};

	void Scan();

	// The extra icon in that slot, or nullptr for a built-in need or an empty slot.
	// Filled once by Scan and never changed after, so pointers into it stay good.
	const Def* Get(Needs::Id a_id);

	// The slot of the extra icon with that name (case-insensitive), if there is one.
	std::optional<Needs::Id> Find(std::string_view a_name);

	// Every page name an extra icon asked for, in the order first seen.
	std::vector<std::string> Pages();
}
