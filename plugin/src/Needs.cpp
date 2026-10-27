#include "Needs.h"

#include "Extras.h"

namespace Needs
{
	namespace
	{
		constexpr std::string_view SURVIVAL = "ccqdrsse001-survivalmode.esl";
		constexpr std::string_view SMI = "SurvivalModeImproved.esp";
		constexpr std::string_view STRESS = "Stress and Fear.esp";
		constexpr std::string_view DIRT = "Dirt and Blood - Dynamic Visuals.esp";
		constexpr std::string_view SUNHELM = "SunHelmSurvival.esp";
		constexpr std::string_view LAST_SEED = "LastSeed.esp";
		constexpr std::string_view INEED = "iNeed.esp";
		constexpr std::string_view FROSTFALL = "Frostfall.esp";
		constexpr std::string_view BATHING = "Bathing in Skyrim.esp";
		constexpr std::string_view DVS = "DVSsurvivaltweaks.esp";
		constexpr std::string_view KYNE = "Tears of Kyne.esp";
		constexpr std::string_view KEEP_CLEAN = "Keep It Clean.esp";
		constexpr std::string_view IMMERSIVE_NEEDS = "Immersive Needs ESL.esp";
		constexpr std::string_view FROSTBITE = "Frostbite.esp";
		constexpr std::string_view DRINK_IT_OFF = "DrinkItOff.esp";
		constexpr std::string_view SIMPLE_NEEDS = "Super Simple Needs.esp";
		constexpr std::string_view RND = "RealisticNeedsandDiseases.esp";
		// Frostfall's four ...ReadOnly globals are its own records, but in Update.esm's
		// form ID space (master index 01 in Frostfall.esp; Update.esm has no such records).
		constexpr std::string_view UPDATE = "Update.esm";

		using Names = std::array<const char*, 6>;  // by our stage, best (0) to worst (5)

		bool Loaded(std::string_view a_plugin)
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			return data && (data->GetLoadedModIndex(a_plugin) || data->GetLoadedLightModIndex(a_plugin));
		}

		// By editor ID first, then by form ID in the plugin that defines it. The
		// form IDs were read from each plugin's own records.
		RE::TESGlobal* FindGlobal(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin)
		{
			auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_editorID);
			const char* how = "editor ID";
			if (!global) {
				global = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(a_localID, a_plugin);
				how = "form ID";
			}
			if (global) {
				logs::info("{}: found by {}", a_editorID, how);
			} else {
				logs::info("{}: not found", a_editorID);
			}
			return global;
		}

		// By form ID first, for globals a mod's own dll reads by form ID (Survival Mode
		// Improved). Blaque23, 2026-10-23, Apostasy: hunger was running but the icon said
		// it was switched off, so the editor ID found a different, same-named global.
		RE::TESGlobal* FindGlobalByID(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin)
		{
			auto*       global = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(a_localID, a_plugin);
			const char* how = "form ID";
			if (auto* named = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_editorID); named && global && named != global) {
				logs::warn("{}: two globals have this editor ID; using {:06X} in {}, not {:08X}", a_editorID, a_localID, a_plugin,
					named->GetFormID());
			}
			if (!global) {
				global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_editorID);
				how = "editor ID";
			}
			if (global) {
				logs::info("{}: found by {}", a_editorID, how);
			} else {
				logs::info("{}: not found", a_editorID);
			}
			return global;
		}

		// The game keeps no editor IDs for spells or quests, so these go by form ID only.
		RE::SpellItem* FindSpell(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin)
		{
			auto* spell = RE::TESDataHandler::GetSingleton()->LookupForm<RE::SpellItem>(a_localID, a_plugin);
			logs::info("{}: {}", a_editorID, spell ? "found" : "not found");
			return spell;
		}

		RE::TESQuest* FindQuest(std::string_view a_editorID, RE::FormID a_localID, std::string_view a_plugin)
		{
			auto* quest = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESQuest>(a_localID, a_plugin);
			logs::info("{}: {}", a_editorID, quest ? "found" : "not found");
			return quest;
		}

		bool Is(const RE::TESGlobal* a_global, float a_value)
		{
			return a_global && a_global->value == a_value;
		}

		bool PlayerHas(RE::SpellItem* a_spell)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return a_spell && player && player->HasSpell(a_spell);
		}

		// Whether anything the player wears has the keyword. Walks the inventory changes
		// itself: CommonLib's GetWornArmor builds a map of the whole inventory and asserts
		// when a save holds two entries for one item (gaodaxia1985's crash on load, 2.3.0).
		bool PlayerWears(RE::BGSKeyword* a_keyword)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* changes = player ? player->GetInventoryChanges(true) : nullptr;
			if (!a_keyword || !changes || !changes->entryList) {
				return false;
			}
			for (const auto* entry : *changes->entryList) {
				const auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
				if (armor && armor->HasKeyword(a_keyword) && entry->IsWorn()) {
					return true;
				}
			}
			return false;
		}

		// How many of the steps the value has reached: value >= step (or > step).
		template <std::size_t N>
		int StepsReached(float a_value, const std::array<float, N>& a_steps, bool a_strict = false)
		{
			int n = 0;
			for (const auto step : a_steps) {
				if (a_strict ? a_value > step : a_value >= step) {
					++n;
				}
			}
			return n;
		}

		// One mod that can feed one icon. read() fills the reading and returns true
		// while the need is live; when it returns false it sets why.
		struct Source
		{
			const char*                  mod{ "" };
			Names                        names{};
			bool                         installed{ false };
			const char*                  fine{ nullptr };  // what "fine" means, when the stage names don't say it well
			std::function<bool(Reading&, std::string& why, std::string& extra)> read;
			// A source still waiting for its first answer (an extra icon's Papyrus call)
			// holds the icon back rather than letting a later source feed it meanwhile.
			std::function<bool()> waiting;
		};

		std::array<std::vector<Source>, kCount> sources;

		std::mutex                      lastLock;
		std::array<Reading, kCount>     last{};
		std::array<std::string, kCount> lastWhy{};
		std::array<std::string, kCount> lastFine{};
		std::array<int, kCount>         loggedStage{};
		std::array<bool, kCount>        loggedAvailable{};
		std::array<const char*, kCount> loggedSource{};

		std::string Joined(const std::vector<std::string>& a_items, const char* a_last)
		{
			std::string out;
			for (std::size_t k = 0; k < a_items.size(); ++k) {
				if (k > 0) {
					out += k + 1 == a_items.size() ? a_last : ", ";
				}
				out += a_items[k];
			}
			return out;
		}

		std::string FineText(Id a_id, const Source& a_source)
		{
			if (a_source.fine) {
				return a_source.fine;
			}
			std::vector<std::string> names;
			for (int stage = 0; stage <= FineUpTo(a_id); ++stage) {
				const std::string name = a_source.names[stage];
				if (std::ranges::find(names, name) == names.end()) {
					names.push_back(name);
				}
			}
			return Joined(names, " or ");
		}

		// One log line whenever an icon's stage, source or availability changes:
		// enough to check the readings against the game without flooding the log.
		void LogChange(Id a_id, const Reading& a_r, const std::string& a_why, const std::string& a_extra)
		{
			if (a_r.available == loggedAvailable[a_id] && (!a_r.available || (a_r.stage == loggedStage[a_id] && a_r.source == loggedSource[a_id]))) {
				return;
			}
			loggedAvailable[a_id] = a_r.available;
			loggedStage[a_id] = a_r.stage;
			loggedSource[a_id] = a_r.source;
			if (!a_r.available) {
				logs::info("{}: hidden ({})", Name(a_id), a_why);
				return;
			}
			// Extra icons' values can be small (an addon picks its own scale), so two places.
			if (IsExtra(a_id)) {
				logs::info("{}: {} ({:.2f} of {:.2f}, stage {}) from {}{}", Name(a_id), a_r.stageName ? a_r.stageName : "", a_r.value, a_r.max, a_r.stage, a_r.source, a_extra);
				return;
			}
			logs::info("{}: {} ({:.0f} of {:.0f}, stage {}) from {}{}", Name(a_id), a_r.stageName ? a_r.stageName : "", a_r.value, a_r.max, a_r.stage, a_r.source, a_extra);
		}

		// --- Survival Mode, with Survival Mode Improved's thresholds -------------

		struct SurvivalNeed
		{
			std::array<float, 5>          fallback;  // fractions of max, only for a missing threshold global
			RE::TESGlobal*                value{ nullptr };
			RE::TESGlobal*                max{ nullptr };
			std::array<RE::TESGlobal*, 5> thresholds{};
			RE::TESGlobal*                smiStage{ nullptr };    // Survival Mode Improved's own stage, logged beside ours
			RE::TESGlobal*                switchedOn{ nullptr };  // Survival Mode Improved's per-need switch
		};

		RE::TESGlobal* modeEnabled{ nullptr };

		// Fallback thresholds are Survival Mode Improved's, as fractions of its maximum.
		// Plain Survival Mode keeps its cold and fatigue steps inside its scripts, so
		// without Survival Mode Improved those two are approximate.
		std::array<SurvivalNeed, 3> survival{ {
			{ { 0.08f, 0.16f, 0.34f, 0.52f, 0.77f } },
			{ { 0.028f, 0.167f, 0.278f, 0.5f, 0.778f } },
			{ { 0.138f, 0.277f, 0.415f, 0.623f, 0.83f } },
		} };

		Source SurvivalSource(Id a_id, Names a_names)
		{
			Source s;
			s.mod = "Survival Mode";
			s.names = a_names;
			auto& need = survival[a_id];
			s.installed = modeEnabled && need.value && need.max;
			s.read = [&need](Reading& r, std::string& why, std::string& extra) {
				if (modeEnabled->value == 0.0f) {
					why = "Survival Mode is off.";
					return false;
				}
				if (need.switchedOn && need.switchedOn->value == 0.0f) {
					why = "This need is switched off in Survival Mode Improved.";
					return false;
				}
				if (need.max->value <= 0.0f) {
					why = "Survival Mode has no maximum for this need yet.";
					return false;
				}
				r.value = need.value->value;
				r.max = need.max->value;
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				r.stage = 0;
				for (std::size_t i = 0; i < need.thresholds.size(); ++i) {
					const float at = need.thresholds[i] ? need.thresholds[i]->value : need.fallback[i] * r.max;
					if (r.value >= at) {
						r.stage = static_cast<int>(i) + 1;
					}
				}
				if (need.smiStage) {
					extra = std::format(", Survival Mode Improved says stage {:.0f}", need.smiStage->value);
				}
				return true;
			};
			return s;
		}

		// --- SunHelm Survival -----------------------------------------------------
		// _SHHungerSystemScript and its three siblings: stage 0 below Stage1, ...,
		// stage 5 from Stage5 up. The Stage values are the quests' own script
		// properties (VMAD) and never change at runtime. Higher is worse. A stopped
		// need keeps its last value, so the quest must be running too.

		RE::TESGlobal* sunHelmOn{ nullptr };  // _SHEnabled, 1 once the mod has started

		Source SunHelmSource(const char* a_need, RE::FormID a_value, RE::FormID a_disabled, RE::FormID a_quest,
			std::array<float, 5> a_steps, Names a_names, RE::TESGlobal* a_extraOff = nullptr, RE::TESGlobal* a_active = nullptr)
		{
			Source s;
			s.mod = "SunHelm";
			s.names = a_names;
			if (!Loaded(SUNHELM)) {
				return s;
			}
			auto* value = FindGlobal(std::format("_SHCurrent{}Level", a_need), a_value, SUNHELM);
			auto* disabled = FindGlobal(std::format("_SH{}ShouldBeDisabled", a_need), a_disabled, SUNHELM);
			auto* quest = FindQuest(std::format("_SH{}Quest", a_need), a_quest, SUNHELM);
			s.installed = value && sunHelmOn;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(sunHelmOn, 1.0f)) {
					why = "SunHelm hasn't started, or is stopped.";
					return false;
				}
				if (Is(disabled, 1.0f) || Is(a_extraOff, 1.0f) || (a_active && !Is(a_active, 1.0f)) || (quest && !quest->IsRunning())) {
					why = "This need is switched off in SunHelm.";
					return false;
				}
				r.value = value->value;
				r.max = a_steps.back();
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				r.stage = StepsReached(r.value, a_steps);
				return true;
			};
			return s;
		}

		// --- Last Seed --------------------------------------------------------------
		// _Seed_AttributeSystem: 0 to 120, higher is worse, bands of 20 with the top
		// of each band in it (IsUpToAndBetween). Each need is on when its setting is 2;
		// the whole mod when LastSeedRunning is 2. Stopping keeps the last value.

		RE::TESGlobal* lastSeedRunning{ nullptr };

		Source LastSeedSource(const char* a_need, RE::FormID a_value, RE::FormID a_enabled, Names a_names)
		{
			Source s;
			s.mod = "Last Seed";
			s.names = a_names;
			if (!Loaded(LAST_SEED)) {
				return s;
			}
			auto* value = FindGlobal(std::format("_Seed_Attribute{}", a_need), a_value, LAST_SEED);
			auto* enabled = FindGlobal(std::format("_Seed_Setting_SystemEnabled_{}", a_need), a_enabled, LAST_SEED);
			s.installed = value && lastSeedRunning;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(lastSeedRunning, 2.0f)) {
					why = "Last Seed isn't running.";
					return false;
				}
				if (enabled && !Is(enabled, 2.0f)) {
					why = "This need is switched off in Last Seed.";
					return false;
				}
				constexpr std::array<float, 5> BAND_TOPS{ 20.0f, 40.0f, 60.0f, 80.0f, 100.0f };
				r.value = value->value;
				r.max = 120.0f;
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				r.stage = StepsReached(r.value, BAND_TOPS, true);
				return true;
			};
			return s;
		}

		// _Seed_VitalitySystem: 0 to 160, lower is worse, eight levels in bands of 20
		// (Dying to Peak Condition), moving toward a target the other needs pull down.
		// The ring fills as it drops. Named by Last Seed's level, coloured on our scale.
		Source LastSeedVitalitySource()
		{
			Source s;
			s.mod = "Last Seed";
			s.names = { "Peak Condition", "Vigorous", "Healthy", "Drained", "Weakened", "Ruined" };
			s.fine = "Peak Condition or Vigorous";
			if (!Loaded(LAST_SEED)) {
				return s;
			}
			auto* value = FindGlobal("_Seed_AttributeVitality", 0x004FC4, LAST_SEED);
			auto* enabled = FindGlobal("_Seed_Setting_SystemEnabled_Vitality", 0x007076, LAST_SEED);
			s.installed = value && lastSeedRunning;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(lastSeedRunning, 2.0f)) {
					why = "Last Seed isn't running.";
					return false;
				}
				if (enabled && !Is(enabled, 2.0f)) {
					why = "Vitality is switched off in Last Seed.";
					return false;
				}
				constexpr std::array<float, 7> BAND_TOPS{ 20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f, 140.0f };
				constexpr std::array           LEVEL_NAMES{ "Dying", "Ruined", "Weakened", "Drained", "Healthy", "Very Healthy", "Vigorous", "Peak Condition" };
				constexpr std::array           LEVEL_STAGE{ 5, 5, 4, 3, 2, 1, 1, 0 };
				const int                      level = StepsReached(value->value, BAND_TOPS, true);
				r.value = value->value;
				r.max = 160.0f;
				r.fraction = std::clamp((r.max - r.value) / r.max, 0.0f, 1.0f);
				r.stage = LEVEL_STAGE[level];
				r.stageName = LEVEL_NAMES[level];
				return true;
			};
			return s;
		}

		RE::SpellItem* iNeedSpell{ nullptr };  // iNeed's _SNPlayerSpell: the player loses it when iNeed is off

		// --- Disease ---------------------------------------------------------------------
		// Any Disease spell running on the player (vanilla, the DLCs, Survival Mode, Last
		// Seed, SunHelm's food poisoning) lights the icon. How bad comes from the mods
		// that stage their diseases, on a common 1-5 scale:
		//   Survival Mode - its stage 2 and 3 spells (the vanilla one stays as stage 1)
		//   Last Seed     - which _Seed_DiseaseSpellsCurrent<Level> list holds the spell
		//                   (filled by script; the mod reads them the same way)
		//   iNeed         - _SNDiseaseStage 1-4; its stage spells are Abilities, so the
		//                   type check misses them and the global is read instead
		// The worst one names the stage.

		Source DiseaseSource()
		{
			Source s;
			s.mod = "Skyrim";
			s.names = { "Healthy", "Diseased", "Diseased", "Diseased", "Diseased", "Diseased" };
			s.fine = "Healthy";
			s.installed = true;

			// Survival Mode's stage 2 and stage 3 spells, by disease.
			std::vector<std::pair<RE::SpellItem*, int>> survivalStages;
			if (Loaded(SURVIVAL)) {
				constexpr std::array<std::pair<RE::FormID, RE::FormID>, 10> STAGES{ {
					{ 0x992, 0x993 }, { 0x984, 0x985 }, { 0x990, 0x991 }, { 0x98C, 0x98D }, { 0x98E, 0x98F },
					{ 0x994, 0x995 }, { 0x996, 0x997 }, { 0x987, 0x988 }, { 0x989, 0x98A }, { 0x98B, 0x986 },
				} };
				auto* data = RE::TESDataHandler::GetSingleton();
				for (const auto& [two, three] : STAGES) {
					survivalStages.emplace_back(data->LookupForm<RE::SpellItem>(two, SURVIVAL), 3);
					survivalStages.emplace_back(data->LookupForm<RE::SpellItem>(three, SURVIVAL), 5);
				}
				logs::info("Survival Mode disease stages: {} of 20 found",
					std::ranges::count_if(survivalStages, [](const auto& p) { return p.first != nullptr; }));
			}
			std::array<RE::BGSListForm*, 5> lastSeedLevels{};
			if (Loaded(LAST_SEED)) {
				auto* data = RE::TESDataHandler::GetSingleton();
				// Last Seed 2026's ESL build renumbered its records: the same lists are 0xA65-0xA69.
				const bool light = data->GetLoadedLightModIndex(LAST_SEED).has_value();
				for (std::uint32_t k = 0; k < lastSeedLevels.size(); ++k) {
					lastSeedLevels[k] = data->LookupForm<RE::BGSListForm>((light ? 0xA65 : 0x176E91) + k, LAST_SEED);
				}
				logs::info("Last Seed disease lists: {} of 5 found",
					std::ranges::count_if(lastSeedLevels, [](auto* l) { return l != nullptr; }));
			}
			RE::TESGlobal* iNeedStage = Loaded(INEED) ? FindGlobal("_SNDiseaseStage", 0x01E2D2, INEED) : nullptr;
			// Realistic Needs and Diseases adds a stage spell per disease beside the disease
			// itself (RND_ProgDisease): Stage0 at infection, Stage1 and Stage2 hours later.
			std::vector<std::pair<RE::SpellItem*, int>> rndStages;
			if (Loaded(RND)) {
				constexpr std::array<std::array<RE::FormID, 6>, 3> STAGES{ {
					{ 0x010678, 0x01067A, 0x01067C, 0x01067E, 0x010680, 0x010682 },
					{ 0x00A530, 0x00F623, 0x00F62D, 0x00F61C, 0x00F0AE, 0x00F0B4 },
					{ 0x00A534, 0x00F625, 0x00F62F, 0x00F61E, 0x00F0AF, 0x00F0B2 },
				} };
				constexpr std::array RND_SEVERITY{ 1, 3, 5 };
				auto* data = RE::TESDataHandler::GetSingleton();
				for (std::size_t k = 0; k < STAGES.size(); ++k) {
					for (const auto id : STAGES[k]) {
						if (auto* spell = data->LookupForm<RE::SpellItem>(id, RND)) {
							rndStages.emplace_back(spell, RND_SEVERITY[k]);
						}
					}
				}
				logs::info("Realistic Needs and Diseases disease stages: {} of 18 found", rndStages.size());
			}

			s.read = [=](Reading& r, std::string&, std::string&) {
				static std::string named;  // the worst disease's name, for stageName
				auto*              player = RE::PlayerCharacter::GetSingleton();
				if (!player) {
					return false;
				}
				int         worst = 0;
				int         count = 0;
				const char* from = "Skyrim";
				named.clear();
				// The diseases whose effects are running on the player, as the Active Effects
				// menu lists them. Not the added-spells list: with SunHelm and the Simonrim mods,
				// Survival Mode's stage spells stay there doing nothing (a "Severe Rockjoint"
				// nobody has, 2.3.0 bug reports), and SunHelm passes its stages on as a cast,
				// which never lands in that list at all (Mirax).
				std::vector<RE::SpellItem*> diseases;
				auto* const                 target = player->AsMagicTarget();
				if (auto* const effects = target ? target->GetActiveEffectList() : nullptr) {
					for (auto* effect : *effects) {
						if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
							continue;
						}
						auto* spell = effect->spell ? effect->spell->As<RE::SpellItem>() : nullptr;
						if (spell && spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease && std::ranges::find(diseases, spell) == diseases.end()) {
							diseases.push_back(spell);
						}
					}
				}
				for (auto* spell : diseases) {
					++count;
					int         severity = 1;
					const char* source = "Skyrim";
					for (const auto& [stage, sev] : survivalStages) {
						if (stage == spell) {
							severity = sev;
							source = "Survival Mode";
						}
					}
					for (int k = 0; k < 5; ++k) {
						if (lastSeedLevels[k] && lastSeedLevels[k]->HasForm(spell)) {
							severity = (std::max)(severity, k + 1);
							source = "Last Seed";
						}
					}
					// A disease no list above knows: go by the stage word its name starts with.
					// Wildlander's stages are Basic, Acute and Severe ("Acute Witbane", Mirax
					// 2.5.0); Survival Mode's own Severe and Crippling are matched above already.
					if (severity == 1 && std::string_view(source) == "Skyrim") {
						const std::string_view name = spell->GetName() ? spell->GetName() : "";
						constexpr std::array<std::pair<std::string_view, int>, 6> WORDS{ {
							{ "Mild ", 1 }, { "Basic ", 1 }, { "Moderate ", 2 }, { "Acute ", 3 }, { "Severe ", 4 }, { "Crippling ", 5 },
						} };
						for (const auto& [word, sev] : WORDS) {
							if (name.starts_with(word)) {
								severity = sev;
								source = "its name";
							}
						}
					}
					if (severity > worst || named.empty()) {
						worst = (std::max)(worst, severity);
						named = spell->GetName();
						from = source;
					}
				}
				if (iNeedStage && PlayerHas(iNeedSpell) && iNeedStage->value >= 1.0f) {
					constexpr std::array INEED_SEVERITY{ 0, 1, 3, 4, 5 };  // Mild, Moderate, Severe, Deathly
					const int sev = INEED_SEVERITY[std::clamp(static_cast<int>(std::lround(iNeedStage->value)), 0, 4)];
					if (sev >= worst) {
						worst = sev;
						named = "Diseased";
						from = "iNeed";
					}
					++count;
				}
				for (const auto& [stage, sev] : rndStages) {
					if (PlayerHas(stage) && sev >= worst) {
						worst = sev;
						from = "Realistic Needs and Diseases";
						if (named.empty()) {
							named = "Diseased";
							count = (std::max)(count, 1);
						}
					}
				}
				constexpr std::array SEVERITY_STAGE{ 0, 2, 3, 4, 5, 5 };
				r.stage = SEVERITY_STAGE[std::clamp(worst, 0, 5)];
				r.value = static_cast<float>(count);
				r.max = 5.0f;
				r.fraction = static_cast<float>(r.stage) / 5.0f;
				if (count > 1) {
					named += std::format(" (+{})", count - 1);
				}
				r.stageName = count > 0 ? named.c_str() : "Healthy";
				r.source = from;
				return true;
			};
			return s;
		}

		// --- iNeed ------------------------------------------------------------------
		// _snquestscript keeps the real 0-110 values in script properties only; the
		// globals hold the stage, -2 (well sated) to 3 (severely hungry), and stages
		// 1-3 depend on hours spent at 0, not on the value. So the icon follows the
		// stage, and the ring fills a fifth per stage. A rate of 0 switches a need
		// off; with the mod off the player loses _SNPlayerSpell, and the stages are
		// all left at -2, so that spell is the only reliable on switch.


		Source INeedSource(const char* a_global, RE::FormID a_stage, const char* a_rate, RE::FormID a_rateID, Names a_names)
		{
			Source s;
			s.mod = "iNeed";
			s.names = a_names;
			if (!Loaded(INEED)) {
				return s;
			}
			auto* stage = FindGlobal(a_global, a_stage, INEED);
			auto* rate = FindGlobal(a_rate, a_rateID, INEED);
			s.installed = stage && iNeedSpell;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!PlayerHas(iNeedSpell)) {
					why = "iNeed is switched off.";
					return false;
				}
				if (Is(rate, 0.0f)) {
					why = "This need is switched off in iNeed.";
					return false;
				}
				r.stage = std::clamp(static_cast<int>(std::lround(stage->value)) + 2, 0, 5);
				r.value = static_cast<float>(r.stage);
				r.max = 5.0f;
				r.fraction = r.value / r.max;
				return true;
			};
			return s;
		}

		// --- Frostfall ----------------------------------------------------------------
		// _Frost_ExposureSystem: exposure 0-120, higher is colder, levels at
		// 20/40/60/80/100. _Frost_WetnessSystem: wetness 0-750, levels over 200/550/700.
		// Both update about every 5 seconds. Running when FrostfallRunning is 2;
		// stopping keeps the last values.

		RE::TESGlobal* frostfallRunning{ nullptr };

		Source FrostfallSource(RE::TESGlobal* a_value, bool a_wetness, Names a_names)
		{
			Source s;
			s.mod = "Frostfall";
			s.names = a_names;
			s.installed = a_value && frostfallRunning;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(frostfallRunning, 2.0f)) {
					why = "Frostfall isn't running.";
					return false;
				}
				r.value = a_value->value;
				if (a_wetness) {
					// Frostfall's wet levels 0-3 (dry, damp, wet, drenched) on our 0-5 scale.
					constexpr std::array<float, 3> WET_OVER{ 200.0f, 550.0f, 700.0f };
					constexpr std::array          WET_STAGE{ 0, 2, 4, 5 };
					r.max = 750.0f;
					r.stage = WET_STAGE[StepsReached(r.value, WET_OVER, true)];
				} else {
					constexpr std::array<float, 5> EXPOSURE_AT{ 20.0f, 40.0f, 60.0f, 80.0f, 100.0f };
					r.max = 120.0f;
					r.stage = StepsReached(r.value, EXPOSURE_AT);
				}
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				return true;
			};
			return s;
		}

		// --- DVSMP Survival Tweaks (LoreRim's thirst) -----------------------------------
		// Survival_ThirstValue against Survival_ThirstMaxValue (600), moved by its own
		// SKSE plugin, which was not available to read: the stage is taken from the
		// stage ability on the player (Survival_ThirstStage0Spell ... 5Spell), and
		// every stage effect needs Survival_ModeEnabled == 1. Not tested in game.

		Source DvsThirstSource()
		{
			Source s;
			s.mod = "DVSMP Survival Tweaks";
			s.names = { "Quenched", "Hydrated", "Parched", "Thirsty", "Dehydrated", "Severely Dehydrated" };
			if (!Loaded(DVS)) {
				return s;
			}
			auto* value = FindGlobal("Survival_ThirstValue", 0x800, DVS);
			auto* max = FindGlobal("Survival_ThirstMaxValue", 0x802, DVS);
			std::array<RE::SpellItem*, 6> stages{};
			for (std::uint32_t k = 0; k < stages.size(); ++k) {
				stages[k] = FindSpell(std::format("Survival_ThirstStage{}Spell", k), 0x80F + k, DVS);
			}
			s.installed = value && modeEnabled;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(modeEnabled, 1.0f)) {
					why = "Survival Mode is off, and this thirst needs it.";
					return false;
				}
				r.value = value->value;
				r.max = max && max->value > 0.0f ? max->value : 600.0f;
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				r.stage = 0;
				for (int k = 0; k < 6; ++k) {
					if (PlayerHas(stages[k])) {
						r.stage = k;
					}
				}
				return true;
			};
			return s;
		}

		// --- Tears of Kyne ---------------------------------------------------------------
		// The thirst value lives only in its SKSE plugin and co-save, so there is no
		// global to read. WaterNeedManager keeps exactly one of five stage spells on the
		// player and takes all five off while its system is off (Manager.cpp,
		// STAGE_SPELL_FORM_IDS; GitHub ItzIvy05/TearsofKyne, 4de14c1). Not tested in game.

		constexpr std::array KYNE_NAMES{ "Quenched", "Sated", "Thirsty", "Parched", "Dehydrated" };

		Source TearsOfKyneSource()
		{
			Source s;
			s.mod = "Tears of Kyne";
			s.names = { "Quenched", "Sated", "Thirsty", "Thirsty", "Parched", "Dehydrated" };
			std::array<RE::SpellItem*, 5> stages{
				FindSpell("IvyThirstQuenchedSpell", 0x90C, KYNE),
				FindSpell("IvyThirstSatedSpell", 0x018, KYNE),
				FindSpell("IvyThirstThirstySpell", 0x019, KYNE),
				FindSpell("IvyThirstParchedSpell", 0x801, KYNE),
				FindSpell("IvyThirstDehydratedSpell", 0x805, KYNE),
			};
			s.installed = std::ranges::any_of(stages, [](auto* x) { return x != nullptr; });
			s.read = [=](Reading& r, std::string& why, std::string&) {
				int level = -1;
				for (int k = 0; k < 5; ++k) {
					if (PlayerHas(stages[k])) {
						level = k;
					}
				}
				if (level < 0) {
					why = "Tears of Kyne is off (no stage on the player; it follows Survival Mode by default).";
					return false;
				}
				constexpr std::array KYNE_STAGE{ 0, 1, 2, 4, 5 };  // its 0-4 on our 0-5 colour scale
				r.value = static_cast<float>(level);
				r.max = 4.0f;
				r.fraction = r.value / r.max;
				r.stage = KYNE_STAGE[level];
				r.stageName = KYNE_NAMES[level];
				return true;
			};
			return s;
		}

		// --- Edmond's Immersive Needs ESL ----------------------------------------------------
		// Its hours since a meal, drink or sleep are private script variables, so there is
		// no value to read: only the stage abilities EN_SurvivalPlayerAliasScript puts on the
		// player (Hungry at 6 h, Starving at 120 h with lethal needs on; the same for thirst
		// at 6 h / 72 h; Tired at 16 h). Its 5 h and 14 h warnings are messages only, so the
		// icon steps from fine straight to stage 3. The three base esp variants share these
		// form IDs. Not tested in game.

		Source ImmersiveNeedsSource(const char* a_need, RE::FormID a_mild, const char* a_mildName, int a_mildStage, RE::FormID a_severe,
			const char* a_severeName, const char* a_fineName, RE::TESGlobal* a_off, float a_offWhen, const char* a_offWhy)
		{
			Source s;
			s.mod = "Immersive Needs";
			s.names = { a_fineName, a_fineName, a_fineName, a_mildName, a_mildName, a_severeName ? a_severeName : a_mildName };
			auto* mild = FindSpell(std::format("Immersive Needs {} spell", a_need), a_mild, IMMERSIVE_NEEDS);
			auto* severe = a_severe ? FindSpell(std::format("Immersive Needs severe {} spell", a_need), a_severe, IMMERSIVE_NEEDS) : nullptr;
			s.installed = mild != nullptr;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (a_off && Is(a_off, a_offWhen)) {
					why = a_offWhy;
					return false;
				}
				r.max = 2.0f;
				if (PlayerHas(severe)) {
					r.value = 2.0f;
					r.stage = 5;
				} else if (PlayerHas(mild)) {
					r.value = 1.0f;
					r.stage = a_mildStage;
				} else {
					r.value = 0.0f;
					r.stage = 0;
				}
				r.fraction = r.stage / 5.0f;
				return true;
			};
			return s;
		}

		// --- Values kept only in a mod's own scripts ---------------------------------------------
		// Some mods keep a value in a script variable and nowhere else. The VM is read on the
		// game's main thread, from an SKSE task at most once a second, and the icon shows the
		// latest. The script is the one bound to the quest itself (alias -1) or to the quest's
		// alias with that ID. An auto property's variable is "::Name_var".

		struct ScriptWatch
		{
			RE::TESQuest*                         quest{ nullptr };
			int                                   alias{ -1 };
			std::string                           script;
			std::vector<std::string>              variables;
			std::mutex                            lock;
			std::vector<std::optional<float>>     values;
			bool                                  inFlight{ false };
			bool                                  answered{ false };
			int                                   logged{ -1 };  // -1 nothing yet, 0 not bound, 1 bound
			std::chrono::steady_clock::time_point asked{};
		};

		std::shared_ptr<ScriptWatch> Watch(RE::TESQuest* a_quest, int a_alias, std::string a_script, std::vector<std::string> a_variables)
		{
			auto watch = std::make_shared<ScriptWatch>();
			watch->quest = a_quest;
			watch->alias = a_alias;
			watch->script = std::move(a_script);
			watch->variables = std::move(a_variables);
			watch->values.resize(watch->variables.size());
			return watch;
		}

		// A name with no variable of its own is tried as an auto property ("::Name_var"):
		// a mod's other scripts read both the same way, so code that reads it can't tell.
		std::optional<float> ScriptNumber(const RE::BSScript::Object& a_object, const std::string& a_name)
		{
			const auto* var = a_object.GetVariable(a_name.c_str());
			if (!var && !a_name.starts_with("::")) {
				var = a_object.GetVariable(std::format("::{}_var", a_name).c_str());
			}
			if (!var) {
				return std::nullopt;
			}
			if (var->IsFloat()) {
				return var->GetFloat();
			}
			if (var->IsInt()) {
				return static_cast<float>(var->GetSInt());
			}
			if (var->IsBool()) {
				return var->GetBool() ? 1.0f : 0.0f;
			}
			return std::nullopt;
		}

		void ReadScript(const std::shared_ptr<ScriptWatch>& a_watch)
		{
			std::vector<std::optional<float>> got(a_watch->variables.size());
			bool                              bound = false;
			auto*                             vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (vm && a_watch->quest) {
				auto*         policy = vm->GetObjectHandlePolicy();
				RE::VMHandle  handle = policy->EmptyHandle();
				if (a_watch->alias == -2) {
					// The alias the player fills, wherever it is (Apropos2 adds actors as it meets them).
					auto* player = RE::PlayerCharacter::GetSingleton();
					for (auto* alias : a_watch->quest->aliases) {
						auto* ref = alias && alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID ? static_cast<RE::BGSRefAlias*>(alias) : nullptr;
						if (ref && player && ref->GetReference() == player) {
							handle = policy->GetHandleForObject(RE::BGSRefAlias::VMTYPEID, ref);
							break;
						}
					}
				} else if (a_watch->alias < 0) {
					handle = policy->GetHandleForObject(RE::TESQuest::FORMTYPE, a_watch->quest);
				} else {
					for (auto* alias : a_watch->quest->aliases) {
						if (alias && alias->aliasID == static_cast<std::uint32_t>(a_watch->alias)) {
							handle = policy->GetHandleForObject(RE::BGSRefAlias::VMTYPEID, alias);
							break;
						}
					}
				}
				RE::BSTSmartPointer<RE::BSScript::Object> object;
				if (handle != policy->EmptyHandle() && vm->FindBoundObject(handle, a_watch->script.c_str(), object) && object) {
					bound = true;
					for (std::size_t k = 0; k < got.size(); ++k) {
						got[k] = ScriptNumber(*object, a_watch->variables[k]);
					}
				}
			}
			std::scoped_lock l(a_watch->lock);
			a_watch->values = std::move(got);
			a_watch->inFlight = false;
			a_watch->answered = true;
			if (a_watch->logged != static_cast<int>(bound)) {
				a_watch->logged = static_cast<int>(bound);
				if (!bound) {
					logs::info("{}: not bound yet", a_watch->script);
				} else {
					for (std::size_t k = 0; k < a_watch->values.size(); ++k) {
						const auto& v = a_watch->values[k];
						logs::info("{}.{}: {}", a_watch->script, a_watch->variables[k], v ? std::format("{:.3f}", *v) : "not found");
					}
				}
			}
		}

		// The latest values, asking for fresh ones once they're a second old.
		std::vector<std::optional<float>> Latest(const std::shared_ptr<ScriptWatch>& a_watch)
		{
			const auto now = std::chrono::steady_clock::now();
			std::scoped_lock l(a_watch->lock);
			if (!a_watch->inFlight && now - a_watch->asked >= 1s) {
				a_watch->inFlight = true;
				a_watch->asked = now;
				SKSE::GetTaskInterface()->AddTask([a_watch] { ReadScript(a_watch); });
			}
			return a_watch->values;
		}

		bool Answered(const std::shared_ptr<ScriptWatch>& a_watch)
		{
			std::scoped_lock l(a_watch->lock);
			return a_watch->answered;
		}

		// --- Super Simple Needs (Nexus 3892) -----------------------------------------------------
		// mzin_SSN_PlayerAlias keeps when you last ate and slept (in game days) in private
		// variables, checked once a game hour: "getting hungry" at 10 h, then the Hungry
		// ability at 12 h; getting sleepy at 20 h, Sleepy at 24 h. Nothing past that. The hours
		// come from those variables; if they can't be read, the ability alone gives Hungry or
		// Sleepy. No on/off switch. Read from 1.00's script; not tested in game.

		Source SimpleNeedsSource(const char* a_variable, const char* a_spellName, RE::FormID a_spell, float a_warn, float a_at, Names a_names)
		{
			Source s;
			s.mod = "Super Simple Needs";
			s.names = a_names;
			if (!Loaded(SIMPLE_NEEDS)) {
				return s;
			}
			auto* quest = FindQuest("mzin_SSN_Quest", 0x005901, SIMPLE_NEEDS);
			auto* spell = FindSpell(a_spellName, a_spell, SIMPLE_NEEDS);
			if (!quest && !spell) {
				return s;
			}
			auto watch = quest ? Watch(quest, 0, "mzin_SSN_PlayerAlias", { a_variable }) : nullptr;
			s.installed = true;
			s.waiting = [watch] { return watch && !Answered(watch); };
			s.read = [=](Reading& r, std::string&, std::string&) {
				r.max = a_at;
				r.value = 0.0f;
				r.stage = 0;
				const auto got = watch ? Latest(watch) : std::vector<std::optional<float>>{};
				auto*      calendar = RE::Calendar::GetSingleton();
				if (!got.empty() && got[0] && *got[0] > 0.0f && calendar) {
					r.value = (std::max)((calendar->GetDaysPassed() - *got[0]) * 24.0f, 0.0f);
					r.stage = StepsReached(r.value, std::array{ a_warn / 2.0f, a_warn, a_at, a_at, 1.0e9f });
				}
				if (PlayerHas(spell)) {
					r.stage = 4;
					r.value = (std::max)(r.value, a_at);
				}
				r.fraction = (std::min)(r.value / r.max, 1.0f);
				return true;
			};
			return s;
		}

		// --- Realistic Needs and Diseases (Nexus 23799) -------------------------------------------
		// RND_HungerPoints, RND_ThirstPoints and RND_SleepPoints, 0 to 240, higher is worse,
		// moved once a game hour by its timer effects and straight away by eating, drinking
		// and sleeping. The cut-offs are its own stage globals, which its MCM never changes.
		// The whole mod is off (RND_State 0) until it's started from its MCM, and stopping it
		// leaves the points where they were. Read from 2.3.6's esp and scripts; not tested in game.

		RE::TESGlobal* rndOn{ nullptr };  // RND_State, 1 while the mod is running

		Source RndSource(const char* a_global, RE::FormID a_id, std::array<float, 5> a_steps, Names a_names)
		{
			Source s;
			s.mod = "Realistic Needs and Diseases";
			s.names = a_names;
			if (!Loaded(RND)) {
				return s;
			}
			auto* value = FindGlobal(a_global, a_id, RND);
			s.installed = value && rndOn;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(rndOn, 1.0f)) {
					why = "Realistic Needs and Diseases isn't started (it starts from its MCM).";
					return false;
				}
				r.value = value->value;
				r.max = 240.0f;
				r.fraction = std::clamp(r.value / r.max, 0.0f, 1.0f);
				r.stage = StepsReached(r.value, a_steps);
				return true;
			};
			return s;
		}

		// --- Frostbite (Nexus 27693) --------------------------------------------------------------
		// Body temperature is _JSW_FB_HandlerScript's BodyTemp, an auto property on the player
		// alias of _JSW_FB_HandlerQuest: 1 fully warm down to 0, moved every few seconds while
		// the mod is on. Nothing else keeps it. Regen stops at _JSW_FB_RegenDebuff (0.5) and
		// damage starts at _JSW_FB_DamageDebuff (0.2), both MCM sliders; those are stages 3 and
		// 5 here, the rest are ours. Read from 2.2.5's scripts; not tested in game.

		Source FrostbiteSource()
		{
			Source s;
			s.mod = "Frostbite";
			s.names = { "Warm", "Comfortable", "Chilly", "Very Cold", "Freezing", "Freezing to Death" };
			if (!Loaded(FROSTBITE)) {
				return s;
			}
			auto* quest = FindQuest("_JSW_FB_HandlerQuest", 0x005901, FROSTBITE);
			if (!quest) {
				return s;
			}
			auto* enabled = FindGlobal("_JSW_FB_Enabled", 0x023F0F, FROSTBITE);
			auto* regen = FindGlobal("_JSW_FB_RegenDebuff", 0x029021, FROSTBITE);
			auto* damage = FindGlobal("_JSW_FB_DamageDebuff", 0x029020, FROSTBITE);
			auto  watch = Watch(quest, 0, "_JSW_FB_HandlerScript", { "::BodyTemp_var" });
			s.installed = true;
			s.waiting = [watch] { return !Answered(watch); };
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (enabled && enabled->value != 1.0f) {
					why = "Frostbite is switched off.";
					return false;
				}
				const auto got = Latest(watch);
				if (!got[0]) {
					why = "Frostbite's body temperature can't be read yet.";
					return false;
				}
				const float stops = regen ? 100.0f * (1.0f - regen->value) : 50.0f;
				const float hurts = damage ? 100.0f * (1.0f - damage->value) : 80.0f;
				r.value = 100.0f * (1.0f - std::clamp(*got[0], 0.0f, 1.0f));
				r.max = 100.0f;
				r.fraction = r.value / r.max;
				r.stage = StepsReached(r.value, std::array{ 1.0f, stops / 2.0f, stops, (stops + hurts) / 2.0f, hurts });
				return true;
			};
			return s;
		}

		// --- Drink It Off (Nexus 54498) -----------------------------------------------------------
		// Combat stress: DrinkItOff_Quest's StressLevel, a private script variable, goes up with
		// every hit taken and only drinking brings it down. Its threshold is 600 times
		// StressMultiplier: at 100% of it the Minor Combat Stress abilities go on, at 200% the
		// Major ones. Shown as a percent of the threshold, those two are stages 3 and 5. If the
		// variable can't be read, the abilities still give those two stages. Read from 1.2's
		// scripts; not tested in game.

		Source DrinkItOffSource()
		{
			Source s;
			s.mod = "Drink It Off";
			s.names = { "No combat stress", "Low", "Building", "Minor Combat Stress", "Heavy", "Major Combat Stress" };
			if (!Loaded(DRINK_IT_OFF)) {
				return s;
			}
			auto* quest = FindQuest("DrinkItOff", 0x0036C8, DRINK_IT_OFF);
			auto* minor = FindSpell("DrinkItOff_StressEffect_Low_Magic", 0x006591, DRINK_IT_OFF);
			auto* major = FindSpell("DrinkItOff_StressEffect_High_Magic", 0x006594, DRINK_IT_OFF);
			if (!quest && !minor) {
				return s;
			}
			auto watch = quest ? Watch(quest, -1, "DrinkItOff_Quest", { "StressLevel", "::StressMultiplier_var", "::ModActive_var" }) : nullptr;
			s.installed = true;
			s.waiting = [watch] { return watch && !Answered(watch); };
			s.read = [=](Reading& r, std::string& why, std::string&) {
				const auto got = watch ? Latest(watch) : std::vector<std::optional<float>>{};
				if (!got.empty() && got[2] && *got[2] == 0.0f) {
					why = "Drink It Off is switched off.";
					return false;
				}
				r.max = 200.0f;
				if (!got.empty() && got[0]) {
					const float threshold = 600.0f * (got[1] && *got[1] > 0.0f ? *got[1] : 1.0f);
					r.value = 100.0f * (std::max)(*got[0], 0.0f) / threshold;
					r.stage = StepsReached(r.value, std::array{ 1.0f, 50.0f, 100.0f, 150.0f, 200.0f });
				} else {
					r.stage = PlayerHas(major) ? 5 : PlayerHas(minor) ? 3 : 0;
					r.value = r.stage * 40.0f;
				}
				r.fraction = (std::min)(r.value / r.max, 1.0f);
				return true;
			};
			return s;
		}

		// --- Stress and Fear --------------------------------------------------------------

		Source StressSource()
		{
			Source s;
			s.mod = "Stress and Fear";
			// StressAndPhobiasScript's own steps: > 25, > 45, > 65, > 85.
			s.names = { "No stress", "Low, no effects", "Over 25", "Over 45", "Over 65", "Over 85" };
			s.fine = "25 or under";
			auto* total = FindGlobal("Stress_Total", 0x801, STRESS);
			auto* enabled = FindGlobal("Stress_Enabled", 0x8A5, STRESS);
			s.installed = total != nullptr;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (Is(enabled, 0.0f)) {
					why = "Stress is switched off in Stress and Fear.";
					return false;
				}
				constexpr std::array<float, 5> ABOVE{ 0.0f, 25.0f, 45.0f, 65.0f, 85.0f };
				r.value = total->value;
				r.max = 100.0f;
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				r.stage = StepsReached(r.value, ABOVE, true);
				return true;
			};
			return s;
		}

		// --- Dirt and Blood, Bathing in Skyrim ------------------------------------------------

		constexpr std::array DIRT_NAMES{ "Clean", "Slightly Dirty", "Somewhat Dirty", "Quite Dirty", "Covered in Dirt" };
		constexpr std::array BLOOD_NAMES{ "Clean", "Bloodstains", "Large Bloodstains", "Quite Bloody", "Covered in Blood" };

		Source DirtAndBloodSource()
		{
			Source s;
			s.mod = "Dirt and Blood";
			s.names = { "Clean", "Clean", "Slightly Dirty", "Somewhat Dirty", "Quite Dirty", "Covered in Dirt" };
			s.fine = "Clean or Slightly Dirty";
			std::array<RE::SpellItem*, 4> dirt{
				FindSpell("Dirty_Spell_Dirt1", 0x806, DIRT),
				FindSpell("Dirty_Spell_Dirt2", 0x807, DIRT),
				FindSpell("Dirty_Spell_Dirt3", 0x808, DIRT),
				FindSpell("Dirty_Spell_Dirt4", 0x838, DIRT),
			};
			std::array<RE::SpellItem*, 4> blood{
				FindSpell("Dirty_Spell_Blood1", 0x809, DIRT),
				FindSpell("Dirty_Spell_Blood2", 0x80A, DIRT),
				FindSpell("Dirty_Spell_Blood3", 0x80B, DIRT),
				FindSpell("Dirty_Spell_Blood4", 0x839, DIRT),
			};
			s.installed = std::ranges::any_of(dirt, [](auto* x) { return x != nullptr; });
			s.read = [=](Reading& r, std::string&, std::string&) {
				// Dirt and Blood gives the player one of these abilities at a time and
				// turns blood into dirt as it dries. Take the worse of the two anyway.
				int d = 0;
				int b = 0;
				for (int i = 0; i < 4; ++i) {
					if (PlayerHas(dirt[i])) {
						d = i + 1;
					}
					if (PlayerHas(blood[i])) {
						b = i + 1;
					}
				}
				constexpr std::array DIRT_STAGE{ 0, 2, 3, 4, 5 };  // Dirt and Blood's 0-4 on our 0-5 colour scale
				const int level = (std::max)(d, b);
				r.value = static_cast<float>(level);
				r.max = 4.0f;
				r.fraction = r.value / r.max;
				r.stage = DIRT_STAGE[level];
				r.stageName = b > d ? BLOOD_NAMES[b] : DIRT_NAMES[d];
				return true;
			};
			return s;
		}

		// mzinBatheQuest.UpdateActorDirtPercent: 0.0 to 1.0, higher is dirtier. The
		// tier is the highest threshold reached; the MCM can move the thresholds, so
		// they are read live. On when mzinBathingInSkyrimEnabled is 1.
		Source BathingSource()
		{
			Source s;
			s.mod = "Bathing in Skyrim";
			s.names = { "Clean", "Not Dirty", "Not Dirty", "Slightly Dirty", "Quite Dirty", "Filthy" };
			if (!Loaded(BATHING)) {
				return s;
			}
			auto*                         value = FindGlobal("mzinDirtinessPercentage", 0xDA8, BATHING);
			auto*                         enabled = FindGlobal("mzinBathingInSkyrimEnabled", 0x00C, BATHING);
			std::array<RE::TESGlobal*, 4> tiers{
				FindGlobal("mzinDirtinessTier1", 0xDAA, BATHING),
				FindGlobal("mzinDirtinessTier2", 0xDAB, BATHING),
				FindGlobal("mzinDirtinessTier3", 0xDAC, BATHING),
				FindGlobal("mzinDirtinessTier4", 0x06F, BATHING),
			};
			s.installed = value && enabled;
			s.read = [=](Reading& r, std::string& why, std::string&) {
				if (!Is(enabled, 1.0f)) {
					why = "Bathing in Skyrim is switched off.";
					return false;
				}
				constexpr std::array          DEFAULT_TIERS{ 0.20f, 0.60f, 0.98f, 1.0f };
				constexpr std::array          TIER_STAGE{ 0, 1, 3, 4, 5 };  // its tiers 0-4 on our 0-5 scale
				int                           tier = 0;
				r.value = value->value;
				for (std::size_t k = 0; k < tiers.size(); ++k) {
					if (r.value >= (tiers[k] ? tiers[k]->value : DEFAULT_TIERS[k])) {
						tier = static_cast<int>(k) + 1;
					}
				}
				r.max = 1.0f;
				r.fraction = std::clamp(r.value, 0.0f, 1.0f);
				r.stage = TIER_STAGE[tier];
				return true;
			};
			return s;
		}

		// SBBathBeingDirtyMGEFScript (Keep It Clean 3.15): hours since the last wash are
		// (SBBathLastDirtyUpdateTimeGLOBAL - SBBathLastWashingTimeGLOBAL) * 24, set on its
		// own tick with the location time already folded in. Past the soap length it is
		// "no longer fresh", past TimetobeDirty dirty, past TimetobeVeryDirty very dirty;
		// the MCM can move all three. Read from the globals, not its stage spells, since
		// the spells are skipped when its debuffs are switched off. Until the player
		// washes once the mod does nothing.
		Source KeepItCleanSource()
		{
			Source s;
			s.mod = "Keep It Clean";
			s.names = { "Clean", "Clean", "No longer fresh", "No longer fresh", "Dirty", "Very dirty" };
			s.fine = "Clean or No longer fresh";
			if (!Loaded(KEEP_CLEAN)) {
				return s;
			}
			auto* washed = FindGlobal("SBBathLastWashingTimeGLOBAL", 0x0EA4EE, KEEP_CLEAN);
			auto* updated = FindGlobal("SBBathLastDirtyUpdateTimeGLOBAL", 0x0ED3B8, KEEP_CLEAN);
			auto* soap = FindGlobal("SBBathSoapEffectLengthGLOBAL", 0x0F314B, KEEP_CLEAN);
			auto* dirty = FindGlobal("SBBathTimetobeDirtyGLOBAL", 0x0F314C, KEEP_CLEAN);
			auto* veryDirty = FindGlobal("SBBathTimetobeVeryDirtyGLOBAL", 0x16F9F6, KEEP_CLEAN);
			s.installed = washed && updated;
			s.read = [=](Reading& r, std::string&, std::string&) {
				const float soapAt = soap ? soap->value : 4.0f;
				const float dirtyAt = dirty ? dirty->value : 16.0f;
				const float veryDirtyAt = veryDirty ? veryDirty->value : 24.0f;
				r.value = 0.0f;
				// Its own test: both times must be past the first game hour.
				if (static_cast<int>(washed->value * 24.0f) != 0 && static_cast<int>(updated->value * 24.0f) != 0) {
					r.value = (std::max)((updated->value - washed->value) * 24.0f, 0.0f);
				}
				r.max = veryDirtyAt > 0.0f ? veryDirtyAt : 24.0f;
				r.fraction = r.value / r.max;
				r.stage = r.value > veryDirtyAt ? 5 : r.value > dirtyAt ? 4 : r.value > soapAt ? 2 : 0;
				return true;
			};
			return s;
		}

		// --- Encumbrance -------------------------------------------------------------------

		Source EncumbranceSource()
		{
			Source s;
			s.mod = "Skyrim";
			s.names = { "Light", "Half full", "Heavy", "Nearly full", "Full", "Over-encumbered" };
			s.fine = "under 90% full";
			s.installed = true;
			s.read = [](Reading& r, std::string& why, std::string&) {
				auto*       player = RE::PlayerCharacter::GetSingleton();
				auto*       values = player ? player->AsActorValueOwner() : nullptr;
				const float capacity = values ? values->GetActorValue(RE::ActorValue::kCarryWeight) : 0.0f;
				if (capacity <= 0.0f) {
					why = "Load a game to see this.";
					return false;
				}
				r.value = values->GetActorValue(RE::ActorValue::kInventoryWeight);
				r.max = capacity;
				r.fraction = (std::max)(r.value, 0.0f) / r.max;
				if (r.value > r.max) {
					r.stage = 5;
				} else if (r.fraction >= 0.95f) {
					r.stage = 4;
				} else if (r.fraction >= 0.9f) {
					r.stage = 3;
				} else if (r.fraction >= 0.75f) {
					r.stage = 2;
				} else if (r.fraction >= 0.5f) {
					r.stage = 1;
				}
				return true;
			};
			return s;
		}

		// --- Extra icons from addon files (Extras.h, docs/EXTRA-ICONS.md) -----------------

		// The latest answer from an extra icon's Papyrus function. The VM answers on its
		// own thread a moment after the call, so the icon shows the latest answer.
		struct PapyrusCall
		{
			std::mutex                            lock;
			bool                                  inFlight{ false };
			bool                                  answered{ false };
			bool                                  gaveUp{ false };  // the call itself failed: the script or function isn't there
			std::string                           problem;          // why the last answer was no good, if it wasn't
			std::string                           label;            // Script.Function, for the log
			int                                   logged{ 0 };      // answers written to the log so far
			float                                 value{ 0.0f };
			std::chrono::steady_clock::time_point asked{};
		};

		constexpr auto LOST_ANSWER = 10s;  // ask again if an answer never came

		class PapyrusAnswer : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			explicit PapyrusAnswer(std::shared_ptr<PapyrusCall> a_call) :
				call(std::move(a_call))
			{}

			void operator()(RE::BSScript::Variable a_result) override
			{
				std::scoped_lock l(call->lock);
				call->inFlight = false;
				// The first few answers go in the log as they came, type and all, so an
				// addon's author can see what their function really gives back.
				if (call->logged < 3) {
					++call->logged;
					const auto type = a_result.IsFloat() ? "float" : a_result.IsInt() ? "int" : a_result.IsBool() ? "bool" : "not a number";
					const auto shown = a_result.IsFloat() ? a_result.GetFloat() : a_result.IsInt() ? static_cast<float>(a_result.GetSInt()) :
					                   a_result.IsBool()  ? (a_result.GetBool() ? 1.0f : 0.0f) :
					                                        0.0f;
					logs::info("{} answered: {} {:.3f}", call->label, type, shown);
				}
				if (a_result.IsFloat()) {
					call->value = a_result.GetFloat();
				} else if (a_result.IsInt()) {
					call->value = static_cast<float>(a_result.GetSInt());
				} else if (a_result.IsBool()) {
					call->value = a_result.GetBool() ? 1.0f : 0.0f;
				} else {
					call->problem = "its function gave back no number";
					return;
				}
				call->problem.clear();
				call->answered = true;
			}

			void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

		private:
			std::shared_ptr<PapyrusCall> call;
		};

		// One Papyrus function to ask: sScript.sFunction(player) by default.
		struct PapyrusRequest
		{
			std::string                script;
			std::string                function;
			bool                       passPlayer{ true };
			bool                       onPlayer{ false };  // the player's own function, as sScript (Actor.GetLightLevel)
			std::string                key;                // sScript.sFunction(player, key, -1.0) (StorageUtil.GetFloatValue)
			std::optional<std::int32_t> intDefault;        // with key: sScript.sFunction(player, key, this) (StorageUtil.GetIntValue)
			bool                       noActor{ false };   // with key: None instead of the player
			RE::TESQuest*              quest{ nullptr };   // the function of the quest's own script, not a global one
			std::optional<std::string> arg;                // a text after the player (SexLab Parasites' parasite names)
		};

		std::string Describe(const PapyrusRequest& a_request)
		{
			std::string args;
			if (!a_request.key.empty()) {
				args = std::format("{}, {}", a_request.noActor ? "None" : "player", a_request.key);
			} else if (!a_request.onPlayer) {
				args = a_request.passPlayer ? "player" : "";
				if (a_request.arg) {
					args += std::format("{}\"{}\"", args.empty() ? "" : ", ", *a_request.arg);
				}
			}
			const auto on = a_request.onPlayer ? "the player's "s : a_request.quest ? std::format("quest {:08X}'s ", a_request.quest->GetFormID()) : ""s;
			return std::format("{}{}.{}({})", on, a_request.script, a_request.function, args);
		}

		// Sends the request from a SKSE task, on the game's main thread.
		void Ask(std::shared_ptr<PapyrusCall> a_call, PapyrusRequest a_request)
		{
			SKSE::GetTaskInterface()->AddTask([a_call, r = std::move(a_request)]() {
				auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
				auto* player = RE::PlayerCharacter::GetSingleton();
				bool  sent = false;
				if (vm && player) {
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> answer(new PapyrusAnswer(a_call));
					auto*                                                    policy = vm->GetObjectHandlePolicy();
					if (r.onPlayer) {
						const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, player);
						sent = vm->DispatchMethodCall(handle, r.script.c_str(), r.function.c_str(), RE::MakeFunctionArguments(), answer);
					} else if (!r.key.empty()) {
						auto* form = r.noActor ? nullptr : static_cast<RE::TESForm*>(player);
						auto* args = r.intDefault ? RE::MakeFunctionArguments(std::move(form), RE::BSFixedString(r.key), std::int32_t{ *r.intDefault }) :
						                            RE::MakeFunctionArguments(std::move(form), RE::BSFixedString(r.key), -1.0f);
						sent = vm->DispatchStaticCall(r.script.c_str(), r.function.c_str(), args, answer);
					} else {
						const auto handle = r.quest ? policy->GetHandleForObject(RE::TESQuest::FORMTYPE, r.quest) : policy->EmptyHandle();
						if (!r.quest || handle != policy->EmptyHandle()) {
							auto* actor = static_cast<RE::Actor*>(player);
							RE::BSScript::IFunctionArguments* args =
								r.passPlayer && r.arg ? RE::MakeFunctionArguments(std::move(actor), RE::BSFixedString(*r.arg)) :
								r.passPlayer          ? RE::MakeFunctionArguments(std::move(actor)) :
								r.arg                 ? RE::MakeFunctionArguments(RE::BSFixedString(*r.arg)) :
								                        RE::MakeFunctionArguments();
							sent = r.quest ? vm->DispatchMethodCall(handle, r.script.c_str(), r.function.c_str(), args, answer) :
							                 vm->DispatchStaticCall(r.script.c_str(), r.function.c_str(), args, answer);
						}
					}
				}
				if (!sent) {
					std::scoped_lock l(a_call->lock);
					a_call->inFlight = false;
					a_call->gaveUp = true;
					a_call->problem = std::format("{} could not be called", Describe(r));
					logs::warn("{}", a_call->problem);
				}
			});
		}

		// The latest answer to the request, asking again every a_every. False, with why,
		// until there's a good one.
		std::function<bool(float&, std::string&)> Polled(std::shared_ptr<PapyrusCall> a_call, PapyrusRequest a_request,
			std::chrono::duration<float> a_every, std::string a_mod)
		{
			return [a_call, a_request, a_every, a_mod](float& v, std::string& why) {
				const auto now = std::chrono::steady_clock::now();
				bool       ask = false;
				{
					std::scoped_lock l(a_call->lock);
					if (a_call->inFlight && now - a_call->asked > LOST_ANSWER) {
						// No answer at all: stop holding the icon for it, so a later
						// source can feed it, and keep asking.
						a_call->inFlight = false;
						a_call->problem = "no answer from its function";
					}
					if (!a_call->inFlight && !a_call->gaveUp && now - a_call->asked >= a_every) {
						a_call->inFlight = true;
						a_call->asked = now;
						ask = true;
					}
				}
				if (ask) {
					Ask(a_call, a_request);
				}
				std::scoped_lock l(a_call->lock);
				if (a_call->gaveUp || !a_call->problem.empty()) {
					why = std::format("{}: {}.", a_mod, a_call->problem);
					return false;
				}
				if (!a_call->answered) {
					why = std::format("Waiting for {}'s first answer.", a_mod);
					return false;
				}
				v = a_call->value;
				return true;
			};
		}

		// --- Wounds (Nexus 17581), sType = Wounds ------------------------------------------
		// Every injury is an ability _W_QuestScript adds to the player. Four form lists,
		// one per kind, hold one inner list per body part (Head, Torso, Right Arm, Left Arm,
		// Right Leg, Left Leg), indexed by rank. There's no value to read, so this scores what
		// the player has: the worst injury plus a quarter of the rest, 0 (unhurt) to 5. The
		// icon file gives the stages. Read from Wounds 4.0's esp and scripts; not tested in game.

		struct WoundSpell
		{
			RE::SpellItem* spell;
			int            kind;  // 0 broken bone, 1 bruise, 2 concussion, 3 cut
			int            part;
			int            rank;
		};

		std::optional<std::function<bool(float&, std::string&)>> WoundsValue(std::string_view a_plugin)
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			constexpr std::array<RE::FormID, 4> KINDS{ 0x0164EF, 0x0164F5, 0x0164FC, 0x0164FE };
			std::vector<WoundSpell> spells;
			for (int kind = 0; kind < 4; ++kind) {
				auto* outer = data->LookupForm<RE::BGSListForm>(KINDS[kind], a_plugin);
				if (!outer) {
					logs::info("Wounds: injury list {:06X} not found in {}", KINDS[kind], a_plugin);
					return std::nullopt;
				}
				for (int part = 0; part < static_cast<int>(outer->forms.size()); ++part) {
					auto* inner = outer->forms[part] ? outer->forms[part]->As<RE::BGSListForm>() : nullptr;
					if (!inner) {
						continue;
					}
					for (int rank = 0; rank < static_cast<int>(inner->forms.size()); ++rank) {
						if (auto* spell = inner->forms[rank] ? inner->forms[rank]->As<RE::SpellItem>() : nullptr) {
							spells.push_back({ spell, kind, part, rank });
						}
					}
				}
			}
			// A cut's infection, per part from Torso on (the head takes no cuts).
			std::array<RE::TESGlobal*, 5> infection{};
			if (auto* list = data->LookupForm<RE::BGSListForm>(0x01757C, a_plugin)) {
				for (std::uint32_t k = 0; k < infection.size() && k < list->forms.size(); ++k) {
					infection[k] = list->forms[k] ? list->forms[k]->As<RE::TESGlobal>() : nullptr;
				}
			}
			logs::info("Wounds: {} injury spells found in {}", spells.size(), a_plugin);
			if (spells.empty()) {
				return std::nullopt;
			}
			return [spells, infection](float& v, std::string&) {
				std::vector<int> scores;
				for (const auto& w : spells) {
					if (!PlayerHas(w.spell)) {
						continue;
					}
					int s = 0;
					switch (w.kind) {
					case 0:
					case 2:
						s = w.rank + 1;
						break;
					case 1:
						s = w.rank <= 2 ? 1 : w.rank <= 5 ? 2 : 3;
						break;
					default:
						s = 1 + w.rank / 2;
						if (w.part >= 1 && w.part <= 5 && infection[w.part - 1] && infection[w.part - 1]->value >= 3.0f) {
							++s;
						}
						break;
					}
					scores.push_back((std::min)(s, 5));
				}
				if (scores.empty()) {
					v = 0.0f;
					return true;
				}
				std::ranges::sort(scores, std::greater<>{});
				float rest = 0.0f;
				for (std::size_t k = 1; k < scores.size(); ++k) {
					rest += static_cast<float>(scores[k]);
				}
				v = (std::min)(static_cast<float>(scores.front()) + rest / 4.0f, 5.0f);
				return true;
			};
		}

		// One [SourceN] of an addon's icon file, as a reader like the built-in ones.
		Source ExtraSource(const Extras::Def& a_icon, const Extras::SourceDef& a_def)
		{
			Source s;
			s.mod = a_def.mod.c_str();
			for (std::size_t k = 0; k < s.names.size(); ++k) {
				s.names[k] = (a_def.stages[k].empty() ? a_icon.stages[k] : a_def.stages[k]).c_str();
			}
			s.fine = a_icon.fine.empty() ? nullptr : a_icon.fine.c_str();
			const auto plugin = std::ranges::find_if(a_def.plugins, [](const std::string& p) { return Loaded(p); });
			if (!a_def.plugins.empty() && plugin == a_def.plugins.end()) {
				return s;
			}
			const std::string_view in = plugin != a_def.plugins.end() ? std::string_view(*plugin) : ""sv;
			const std::string      what = std::format("{} for {}", a_def.mod, a_icon.name);

			std::function<bool(float&, std::string&)> value;
			if (a_def.type == "global") {
				auto* global = FindGlobal(a_def.editorID.empty() ? std::format("{:06X}", a_def.formID) : a_def.editorID, a_def.formID, in);
				if (!global) {
					return s;
				}
				value = [global](float& v, std::string&) {
					v = global->value;
					return true;
				};
				// sMaxEditorID / iMaxFormID: AlivePeeing keeps the bladder and its size in two globals.
				if (!a_def.maxEditorID.empty() || a_def.maxFormID != 0) {
					auto* most = FindGlobal(a_def.maxEditorID.empty() ? std::format("{:06X}", a_def.maxFormID) : a_def.maxEditorID, a_def.maxFormID, in);
					if (!most) {
						return s;
					}
					value = [global, most, mod = a_def.mod](float& v, std::string& why) {
						if (most->value <= 0.0f) {
							why = std::format("{} has no maximum for this yet.", mod);
							return false;
						}
						v = 100.0f * global->value / most->value;
						return true;
					};
				}
			} else if (a_def.type == "factionrank") {
				auto* faction = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(a_def.formID, in);
				logs::info("{}: faction {:06X} in {} {}", what, a_def.formID, in, faction ? "found" : "not found");
				if (!faction) {
					return s;
				}
				// bNegativeRanks: FM3 Fixes and Updates puts the cycle in ranks under 0.
				value = [faction, negative = a_def.negativeRanks, mod = a_def.mod](float& v, std::string& why) {
					auto*      player = RE::PlayerCharacter::GetSingleton();
					const auto rank = player ? player->GetFactionRank(faction, true) : -2;
					if (negative ? rank == -1 || rank == -2 : rank < 0) {
						why = std::format("{} has no value for the player yet (switched off for them, or not counted yet).", mod);
						return false;
					}
					v = static_cast<float>(rank);
					return true;
				};
			} else if (a_def.type == "actorvalue") {
				const auto av = RE::ActorValueList::LookupActorValueByName(a_def.actorValue.c_str());
				logs::info("{}: actor value {} {}", what, a_def.actorValue, av != RE::ActorValue::kNone ? "found" : "not found");
				if (av == RE::ActorValue::kNone) {
					return s;
				}
				value = [av](float& v, std::string&) {
					auto* player = RE::PlayerCharacter::GetSingleton();
					if (!player) {
						return false;
					}
					v = player->AsActorValueOwner()->GetActorValue(av);
					return true;
				};
			} else if (a_def.type == "papyrus" || a_def.type == "storagefloat" || a_def.type == "questfunction") {
				// StorageFloat: a PapyrusUtil value kept on the player under sKey.
				// QuestFunction: a function of the script on a quest (Fill Her Up's sr_inflateQuest).
				const bool    stored = a_def.type == "storagefloat";
				RE::TESQuest* quest = nullptr;
				if (a_def.type == "questfunction") {
					quest = FindQuest(std::format("{} quest {:06X}", a_icon.name, a_def.formID), a_def.formID, in);
					if (!quest) {
						return s;
					}
				}
				const auto request = [&](const std::string& a_function, std::optional<std::string> a_arg) {
					PapyrusRequest r;
					r.script = stored ? "StorageUtil" : a_def.script;
					r.function = stored ? "GetFloatValue" : a_function;
					r.key = stored ? a_arg.value_or("") : "";
					r.noActor = a_def.noActor;
					r.passPlayer = a_def.passPlayer;
					r.onPlayer = a_def.onPlayer && !quest;
					r.quest = quest;
					r.arg = std::move(a_arg);
					return r;
				};
				std::vector<std::shared_ptr<PapyrusCall>> calls;
				const auto polled = [&](PapyrusRequest a_request) {
					auto call = std::make_shared<PapyrusCall>();
					call->label = Describe(a_request);
					logs::info("{}: will call {} every {:.2f} s", what, call->label, a_def.every);
					calls.push_back(call);
					return Polled(call, std::move(a_request), std::chrono::duration<float>(a_def.every), a_def.mod);
				};
				// sArgs: one call per text, the answers added up (how many parasites).
				std::vector<std::function<bool(float&, std::string&)>> parts;
				if (stored) {
					// sKey can name several (Curse of Life keeps one size per kind of egg).
					for (const auto& key : a_def.keys) {
						parts.push_back(polled(request("", key)));
					}
				} else if (a_def.args.empty() || a_def.onPlayer) {
					parts.push_back(polled(request(a_def.function, std::nullopt)));
				} else {
					for (const auto& arg : a_def.args) {
						parts.push_back(polled(request(a_def.function, arg)));
					}
				}
				// sMaxFunction: Milk Mod Economy gives the milk and its most as two functions.
				std::optional<std::function<bool(float&, std::string&)>> most;
				if (!a_def.maxFunction.empty() && !stored) {
					most = polled(request(a_def.maxFunction, std::nullopt));
				}
				s.waiting = [calls] {
					return std::ranges::any_of(calls, [](const auto& c) {
						std::scoped_lock l(c->lock);
						return !c->answered && !c->gaveUp && c->problem.empty();
					});
				};
				value = [parts, most, stored, mod = a_def.mod](float& v, std::string& why) {
					float sum = 0.0f;
					bool  set = !stored;
					for (const auto& part : parts) {
						float x = 0.0f;
						if (!part(x, why)) {
							return false;
						}
						// A stored value under 0 isn't set: it counts as 0 beside the others.
						if (stored && x < 0.0f) {
							continue;
						}
						set = true;
						sum += x;
					}
					if (!set) {
						why = std::format("{} has no value on the player (its effect isn't on).", mod);
						return false;
					}
					if (most) {
						float m = 0.0f;
						if (!(*most)(m, why)) {
							return false;
						}
						if (m <= 0.0f) {
							why = std::format("{} has no maximum for this yet.", mod);
							return false;
						}
						sum = 100.0f * sum / m;
					}
					v = sum;
					return true;
				};
			} else if (a_def.type == "spells") {
				// The highest value among the listed spells the player has, or 0.
				std::vector<std::pair<RE::SpellItem*, float>> spells;
				for (const auto& [id, v] : a_def.spells) {
					if (auto* spell = FindSpell(std::format("{} spell {:06X}", a_icon.name, id), id, in)) {
						spells.emplace_back(spell, v);
					}
				}
				// sWornKeywords: the Estrus mods also mark a worn parasite with a keyword.
				std::vector<std::pair<RE::BGSKeyword*, float>> keywords;
				for (const auto& [id, v] : a_def.wornKeywords) {
					auto* keyword = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(id, in);
					logs::info("{}: keyword {:06X} {}", what, id, keyword ? "found" : "not found");
					if (keyword) {
						keywords.emplace_back(keyword, v);
					}
				}
				if (spells.empty() && keywords.empty()) {
					return s;
				}
				value = [spells, keywords](float& v, std::string&) {
					v = 0.0f;
					for (const auto& [spell, worth] : spells) {
						if (PlayerHas(spell)) {
							v = (std::max)(v, worth);
						}
					}
					for (const auto& [keyword, worth] : keywords) {
						if (worth > v && PlayerWears(keyword)) {
							v = worth;
						}
					}
					return true;
				};
			} else if (a_def.type == "magiceffect") {
				// The highest value among the listed magic effects active on the player.
				std::vector<std::pair<RE::EffectSetting*, float>> effects;
				for (const auto& [id, v] : a_def.effects) {
					auto* effect = RE::TESDataHandler::GetSingleton()->LookupForm<RE::EffectSetting>(id, in);
					logs::info("{}: magic effect {:06X} {}", what, id, effect ? "found" : "not found");
					if (effect) {
						effects.emplace_back(effect, v);
					}
				}
				// 2.6.0: or a keyword on any effect active on the player, the way the game's own
				// HasMagicEffectKeyword condition asks (Rogues 'n Raiders tags each injury with its
				// tier and each hex and curse with its kind, across hundreds of effects).
				std::vector<std::pair<RE::BGSKeyword*, float>> keywords;
				for (const auto& [id, v] : a_def.effectKeywords) {
					auto* keyword = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(id, in);
					logs::info("{}: effect keyword {:06X} {}", what, id, keyword ? "found" : "not found");
					if (keyword) {
						keywords.emplace_back(keyword, v);
					}
				}
				if (effects.empty() && keywords.empty()) {
					return s;
				}
				value = [effects, keywords](float& v, std::string&) {
					auto* player = RE::PlayerCharacter::GetSingleton();
					auto* target = player ? player->AsMagicTarget() : nullptr;
					v = 0.0f;
					for (const auto& [effect, worth] : effects) {
						if (target && worth > v && target->HasMagicEffect(effect)) {
							v = worth;
						}
					}
					for (const auto& [keyword, worth] : keywords) {
						RE::MagicItem* spell = nullptr;
						if (target && worth > v && target->HasMagicEffectWithKeyword(keyword, &spell)) {
							v = worth;
						}
					}
					return true;
				};
			} else if (a_def.type == "scriptvariable") {
				// The quest by form ID, or by editor ID (quests keep theirs in game).
				auto* quest = a_def.formID != 0 ? FindQuest(std::format("{} quest {:06X}", a_icon.name, a_def.formID), a_def.formID, in) : nullptr;
				if (!quest && !a_def.editorID.empty()) {
					quest = RE::TESForm::LookupByEditorID<RE::TESQuest>(a_def.editorID);
					logs::info("{}: quest {} {}", what, a_def.editorID, quest ? "found" : "not found");
				}
				if (!quest) {
					return s;
				}
				// Several variables: the highest counts (Apropos2's three kinds of wear and tear).
				std::vector<std::string> names;
				std::string              name;
				for (const char c : a_def.variable + ",") {
					if (c != ',') {
						name += std::isspace(static_cast<unsigned char>(c)) ? "" : std::string(1, c);
					} else if (!name.empty()) {
						names.push_back(std::exchange(name, {}));
					}
				}
				auto watch = Watch(quest, a_def.alias, a_def.script, names);
				s.waiting = [watch] { return !Answered(watch); };
				value = [watch, mod = a_def.mod, alias = a_def.alias](float& v, std::string& why) {
					const auto got = Latest(watch);
					bool       any = false;
					for (const auto& g : got) {
						if (g) {
							v = any ? (std::max)(v, *g) : *g;
							any = true;
						}
					}
					if (!any) {
						why = alias == -2 ? std::format("{} isn't tracking you yet.", mod) : std::format("{}'s script value can't be read yet.", mod);
						return false;
					}
					return true;
				};
			} else if (a_def.type == "minineeds") {
				// MiniNeeds (mndController on quest 0x12C4) keeps when you last went and how many
				// hours you can last; the percent is the one mndController hands other mods.
				auto* quest = FindQuest(std::format("{} quest 0012C4", a_icon.name), 0x0012C4, in);
				auto* timeScale = RE::TESForm::LookupByID<RE::TESGlobal>(0x00003A);
				if (!quest || !timeScale) {
					return s;
				}
				const std::string need = _stricmp(a_def.variable.c_str(), "Poop") == 0 ? "Poop" : "Piss";
				auto              watch = Watch(quest, -1, "mndController", { "enable" + need, "lastTime" + need, "time" + need });
				s.waiting = [watch] { return !Answered(watch); };
				value = [watch, timeScale, mod = a_def.mod](float& v, std::string& why) {
					const auto got = Latest(watch);
					if (!got[0] || !got[1] || !got[2]) {
						why = std::format("{}'s script value can't be read yet.", mod);
						return false;
					}
					if (*got[0] == 0.0f) {
						why = std::format("{} has it switched off.", mod);
						return false;
					}
					const auto* calendar = RE::Calendar::GetSingleton();
					if (!calendar || timeScale->value <= 0.0f || *got[2] <= 0.0f) {
						return false;
					}
					v = 20.0f / timeScale->value * 24.0f * (calendar->GetCurrentGameTime() - *got[1]) / *got[2] * 100.0f;
					return true;
				};
			} else if (a_def.type == "slove") {
				// SLO VE: willpower 0-100 as its next scene will find it, worked out from the
				// StorageUtil values it keeps for other mods, the way its next scene start does it.
				std::vector<std::shared_ptr<PapyrusCall>> calls;
				const auto stored = [&](std::string a_key, std::optional<std::int32_t> a_int) {
					PapyrusRequest r;
					r.script = "StorageUtil";
					r.function = a_int ? "GetIntValue" : "GetFloatValue";
					r.key = std::move(a_key);
					r.intDefault = a_int;
					auto call = std::make_shared<PapyrusCall>();
					call->label = Describe(r);
					calls.push_back(call);
					return Polled(call, std::move(r), std::chrono::duration<float>(a_def.every), a_def.mod);
				};
				auto lastScene = stored("SLOVE_LastSexTime", std::nullopt);
				auto broken = stored("SLOVE_BrokenPoints", 0);
				auto rate = stored("SLOVE_RecoverPerHour", 10);
				auto resistance = stored("SLOVE_Resistance", 100);
				logs::info("{}: will read SLO VE's StorageUtil values every {:.2f} s", what, a_def.every);
				s.waiting = [calls] {
					return std::ranges::any_of(calls, [](const auto& c) {
						std::scoped_lock l(c->lock);
						return !c->answered && !c->gaveUp && c->problem.empty();
					});
				};
				value = [lastScene, broken, rate, resistance](float& v, std::string& why) {
					float l = 0.0f, b = 0.0f, r = 0.0f, w = 0.0f;
					if (!lastScene(l, why) || !broken(b, why) || !rate(r, why) || !resistance(w, why)) {
						return false;
					}
					const auto* calendar = RE::Calendar::GetSingleton();
					float       hours = 0.0f;
					if (l >= 0.0f && calendar) {
						hours = (std::max)(std::floor((calendar->GetCurrentGameTime() - l) * 24.0f), 0.0f);
					}
					if (b > 0.0f) {
						v = b > hours ? 0.0f : 100.0f;
					} else {
						v = std::clamp(w + hours * r, 0.0f, 100.0f);
					}
					return true;
				};
			} else if (a_def.type == "wounds") {
				auto wounds = WoundsValue(in);
				if (!wounds) {
					return s;
				}
				value = std::move(*wounds);
			} else {
				return s;  // Extras only lets known types through
			}

			// The mod's own on/off switch: while it's off the source doesn't count, so a later
			// one can feed the icon, or it hides saying why.
			if (!a_def.switchEditorID.empty() || a_def.switchFormID != 0) {
				auto* sw = FindGlobal(a_def.switchEditorID.empty() ? std::format("{:06X}", a_def.switchFormID) : a_def.switchEditorID, a_def.switchFormID, in);
				if (!sw) {
					return s;
				}
				value = [value, sw, on = a_def.switchOn, mod = a_def.mod](float& v, std::string& why) {
					if (sw->value != on) {
						why = std::format("{} has it switched off.", mod);
						return false;
					}
					return value(v, why);
				};
			}

			s.installed = true;
			s.read = [value, max = a_def.max, steps = a_def.steps, higherIsWorse = a_def.higherIsWorse, below = a_def.noValueBelow,
						 remap = a_def.remap, mod = a_def.mod](Reading& r, std::string& why, std::string&) {
				float v = 0.0f;
				if (!value(v, why)) {
					return false;
				}
				// sRemap: Fertility Mode Reloaded packs pregnancy and its cycle into one faction rank.
				for (const auto& m : remap) {
					if (v >= m.from && v <= m.to) {
						v = m.value;
						break;
					}
				}
				// fNoValueBelow: e.g. YPS's hair stage is 0 until hair control is set up.
				if (below && v < *below) {
					why = std::format("{} has nothing to show for this right now.", mod);
					return false;
				}
				r.value = v;
				r.max = max;
				if (higherIsWorse) {
					r.fraction = (std::max)(v, 0.0f) / max;
					r.stage = StepsReached(v, steps);
				} else {
					r.fraction = (std::max)((max - v) / max, 0.0f);
					r.stage = static_cast<int>(std::ranges::count_if(steps, [v](float a_step) { return v <= a_step; }));
				}
				return true;
			};
			return s;
		}

		std::string NoModText(Id a_id)
		{
			std::vector<std::string> mods;
			for (const auto& s : sources[a_id]) {
				mods.emplace_back(s.mod);
			}
			if (mods.empty()) {
				return "Nothing in this build reads this need yet.";
			}
			return std::format("None of the mods for this need are installed ({}), so this icon stays hidden.", Joined(mods, " or "));
		}
	}

	void Init()
	{
		modeEnabled = FindGlobal("Survival_ModeEnabled", 0x826, SURVIVAL);

		auto& hunger = survival[0];
		hunger.value = FindGlobal("Survival_HungerNeedValue", 0x81A, SURVIVAL);
		hunger.max = FindGlobal("Survival_HungerNeedMaxValue", 0x80C, SURVIVAL);
		hunger.thresholds = {
			FindGlobal("Survival_HungerStage1Value", 0x806, SURVIVAL),
			FindGlobal("Survival_HungerStage2Value", 0x802, SURVIVAL),
			FindGlobal("Survival_HungerStage3Value", 0x803, SURVIVAL),
			FindGlobal("Survival_HungerStage4Value", 0x804, SURVIVAL),
			FindGlobal("Survival_HungerStage5Value", 0x805, SURVIVAL),
		};
		hunger.smiStage = FindGlobalByID("SMI_CurrentHungerStage", 0xA14, SMI);
		hunger.switchedOn = FindGlobalByID("SMI_HungerShouldBeEnabled", 0xF27, SMI);

		auto& cold = survival[1];
		cold.value = FindGlobal("Survival_ColdNeedValue", 0x81B, SURVIVAL);
		cold.max = FindGlobal("Survival_ColdNeedMaxValue", 0x84B, SURVIVAL);
		cold.thresholds = {
			FindGlobal("Survival_ColdStage1Value", 0xD20, SMI),
			FindGlobal("Survival_ColdStage2Value", 0xD21, SMI),
			FindGlobal("Survival_ColdStage3Value", 0xD22, SMI),
			FindGlobal("Survival_ColdStage4Value", 0xD23, SMI),
			FindGlobal("Survival_ColdStage5Value", 0xD24, SMI),
		};
		cold.smiStage = FindGlobalByID("SMI_CurrentColdStage", 0xD1E, SMI);
		cold.switchedOn = FindGlobalByID("SMI_ColdShouldBeEnabled", 0xF28, SMI);

		auto& fatigue = survival[2];
		fatigue.value = FindGlobal("Survival_ExhaustionNeedValue", 0x816, SURVIVAL);
		fatigue.max = FindGlobal("Survival_ExhaustionNeedMaxValue", 0x84A, SURVIVAL);
		fatigue.thresholds = {
			FindGlobal("Survival_ExhaustionStage1Value", 0xA17, SMI),
			FindGlobal("Survival_ExhaustionStage2Value", 0xA18, SMI),
			FindGlobal("Survival_ExhaustionStage3Value", 0xA19, SMI),
			FindGlobal("Survival_ExhaustionStage4Value", 0xA1A, SMI),
			FindGlobal("Survival_ExhaustionStage5Value", 0xA1B, SMI),
		};
		fatigue.smiStage = FindGlobalByID("SMI_CurrentExhaustionStage", 0xA1C, SMI);
		fatigue.switchedOn = FindGlobalByID("SMI_ExhaustionShouldBeEnabled", 0xF29, SMI);

		if (Loaded(SUNHELM)) {
			sunHelmOn = FindGlobal("_SHEnabled", 0x02EB63, SUNHELM);
		}
		if (Loaded(LAST_SEED)) {
			lastSeedRunning = FindGlobal("LastSeedRunning", 0x00B162, LAST_SEED);
		}
		if (Loaded(INEED)) {
			iNeedSpell = FindSpell("_SNPlayerSpell", 0x000D63, INEED);
		}
		RE::TESGlobal* exposure = nullptr;
		RE::TESGlobal* wetness = nullptr;
		if (Loaded(FROSTFALL)) {
			frostfallRunning = FindGlobal("FrostfallRunning", 0x06DCFB, FROSTFALL);
			exposure = FindGlobal("FrostfallAttributeExposureReadOnly", 0xCC0E03, UPDATE);
			wetness = FindGlobal("FrostfallAttributeWetnessReadOnly", 0xCC0E04, UPDATE);
		}
		RE::TESGlobal* sunHelmColdOff = nullptr;
		RE::TESGlobal* sunHelmColdActive = nullptr;
		if (Loaded(SUNHELM)) {
			sunHelmColdOff = FindGlobal("_SHForceDisableCold", 0x83132C, SUNHELM);
			sunHelmColdActive = FindGlobal("_SHColdActive", 0x79441D, SUNHELM);
		}

		const Names SURVIVAL_HUNGER{ "Well Fed", "Satisfied", "Peckish", "Hungry", "Famished", "Ravenous" };
		const Names SURVIVAL_COLD{ "Warm", "Comfortable", "Chilly", "Very Cold", "Freezing", "Numb" };
		const Names SURVIVAL_FATIGUE{ "Well Rested", "Rested", "Slightly Tired", "Tired", "Weary", "Debilitated" };

		// Generic editor IDs another mod could share, so by form ID in its own plugin only.
		auto* inThirstEnabled = Loaded(IMMERSIVE_NEEDS) ? RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xA08, IMMERSIVE_NEEDS) : nullptr;
		auto* inSleepless = Loaded(IMMERSIVE_NEEDS) ? RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x8DB, IMMERSIVE_NEEDS) : nullptr;
		rndOn = Loaded(RND) ? FindGlobal("RND_State", 0x012C4C, RND) : nullptr;

		// In the order they are tried: the first one running feeds the icon.
		sources[kHunger] = {
			SurvivalSource(kHunger, SURVIVAL_HUNGER),
			SunHelmSource("Hunger", 0x00EAAE, 0x752707, 0x008EE8, { 40, 80, 140, 240, 360 },
				{ "Well Fed", "Satisfied", "Peckish", "Hungry", "Ravenous", "Starving" }),
			LastSeedSource("Hunger", 0x0029CD, 0x007073, { "Full", "Content", "Hungry", "Very Hungry", "Ravenous", "Starving" }),
			INeedSource("_SNHungerPenalty", 0x0012DB, "_SNHungerRate", 0x005933,
				{ "Well Sated", "Sated", "A Bit Hungry", "Mildly Hungry", "Moderately Hungry", "Severely Hungry" }),
			ImmersiveNeedsSource("hunger", 0x83B, "Hungry", 3, 0x9EF, "Starving", "Fed", nullptr, 0.0f, ""),
			SimpleNeedsSource("LocalLastMealTime", "mzin_SSN_SpellHungry", 0x005906, 10.0f, 12.0f,
				{ "Fed", "Fed", "Getting Hungry", "Hungry", "Hungry", "Hungry" }),
			RndSource("RND_HungerPoints", 0x002884, { 5, 30, 60, 120, 240 },
				{ "Overindulged", "Satiated", "Peckish", "Hungry", "Very Hungry", "Starving" }),
		};
		sources[kCold] = {
			SurvivalSource(kCold, SURVIVAL_COLD),
			SunHelmSource("Cold", 0x6A13C5, 0x75270A, 0x6A13C2, { 25, 150, 250, 450, 700 },
				{ "Warm", "Comfortable", "Chilly", "Cold", "Freezing", "Frigid" }, sunHelmColdOff, sunHelmColdActive),
			FrostfallSource(exposure, false, { "Warm", "Comfortable", "Chilly", "Very Cold", "Freezing", "Freezing to Death" }),
			FrostbiteSource(),
		};
		sources[kFatigue] = {
			SurvivalSource(kFatigue, SURVIVAL_FATIGUE),
			SunHelmSource("Fatigue", 0x021E3F, 0x752709, 0x01E844, { 80, 160, 240, 360, 480 },
				{ "Well Rested", "Rested", "Slightly Tired", "Tired", "Weary", "Exhausted" }),
			LastSeedSource("Fatigue", 0x0029CF, 0x007075, { "Rested", "Sharp", "Tired", "Very Tired", "Exhausted", "Severely Exhausted" }),
			INeedSource("_SNFatiguePenalty", 0x0012DC, "_SNFatigueRate", 0x005932,
				{ "Well Rested", "Rested", "A Bit Fatigued", "Mildly Fatigued", "Moderately Fatigued", "Severely Fatigued" }),
			ImmersiveNeedsSource("fatigue", 0x83C, "Tired", 4, 0, nullptr, "Rested", inSleepless, 1.0f,
				"Immersive Needs has sleep switched off (Sleepless Undeath)."),
			SimpleNeedsSource("LocalLastRestTime", "mzin_SSN_SpellSleepy", 0x005908, 20.0f, 24.0f,
				{ "Rested", "Rested", "Getting Sleepy", "Sleepy", "Sleepy", "Sleepy" }),
			RndSource("RND_SleepPoints", 0x00BAC2, { 15, 30, 60, 120, 240 },
				{ "Not Tired", "Not Tired", "Slightly Tired", "Tired", "Very Tired", "Exhausted" }),
		};
		sources[kThirst] = {
			SunHelmSource("Thirst", 0x05C472, 0x752708, 0x05C470, { 40, 60, 120, 180, 300 },
				{ "Quenched", "Sated", "Thirsty", "Parched", "Dehydrated", "Severely Dehydrated" }),
			LastSeedSource("Thirst", 0x0029CE, 0x007074, { "Quenched", "Refreshed", "Thirsty", "Very Thirsty", "Parched", "Severely Parched" }),
			INeedSource("_SNThirstPenalty", 0x004378, "_SNThirstRate", 0x005934,
				{ "Well Hydrated", "Hydrated", "A Bit Thirsty", "Mildly Thirsty", "Moderately Thirsty", "Severely Thirsty" }),
			DvsThirstSource(),
			TearsOfKyneSource(),
			ImmersiveNeedsSource("thirst", 0x83D, "Thirsty", 3, 0x9F0, "Dehydrated", "Quenched", inThirstEnabled, 0.0f,
				"Immersive Needs has thirst switched off."),
			RndSource("RND_ThirstPoints", 0x002E07, { 20, 40, 80, 160, 240 },
				{ "Quenched", "Quenched", "Slightly Thirsty", "Thirsty", "Very Thirsty", "Dehydrated" }),
		};
		sources[kWetness] = {
			FrostfallSource(wetness, true, { "Dry", "Dry", "Damp", "Damp", "Wet", "Drenched" }),
		};
		sources[kStress] = { StressSource(), DrinkItOffSource() };
		sources[kDirt] = { DirtAndBloodSource(), BathingSource(), KeepItCleanSource() };
		sources[kEncumbrance] = { EncumbranceSource() };
		sources[kVitality] = { LastSeedVitalitySource() };
		sources[kDisease] = { DiseaseSource() };
		for (std::uint32_t id = kBuiltInCount; id < kCount; ++id) {
			if (const auto* icon = Extras::Get(static_cast<Id>(id))) {
				for (const auto& def : icon->sources) {
					sources[id].push_back(ExtraSource(*icon, def));
				}
			}
		}

		loggedStage.fill(-1);
		for (std::uint32_t id = 0; id < kCount; ++id) {
			if (!Used(static_cast<Id>(id))) {
				continue;
			}
			std::vector<std::string> found;
			for (const auto& s : sources[id]) {
				if (s.installed) {
					found.emplace_back(s.mod);
				}
			}
			logs::info("{}: {}", Name(static_cast<Id>(id)), found.empty() ? std::string("no mod for it in this game") : "read from " + Joined(found, ", then "));
			std::scoped_lock l(lastLock);
			lastWhy[id] = found.empty() ? NoModText(static_cast<Id>(id)) : "Waiting for a reading.";
			const auto first = std::ranges::find_if(sources[id], [](const Source& s) { return s.installed; });
			if (first != sources[id].end()) {
				lastFine[id] = FineText(static_cast<Id>(id), *first);
			} else if (!sources[id].empty()) {
				lastFine[id] = FineText(static_cast<Id>(id), sources[id].front());
			}
		}
	}

	Reading Read(Id a_id)
	{
		Reading r;
		if (a_id >= kCount) {
			return r;
		}
		std::string why;
		std::string extra;
		const Source* used = nullptr;
		std::vector<std::string> off;
		for (const auto& s : sources[a_id]) {
			if (!s.installed) {
				continue;
			}
			Reading attempt;
			std::string reason;
			if (s.read(attempt, reason, extra)) {
				r = attempt;
				r.available = true;
				if (!r.source || !*r.source) {
					r.source = s.mod;
				}
				r.stage = std::clamp(r.stage, 0, 5);
				if (!r.stageName) {
					r.stageName = s.names[r.stage];
				}
				used = &s;
				break;
			}
			off.push_back(reason);
			if (s.waiting && s.waiting()) {
				break;
			}
		}
		if (!used) {
			why = off.empty() ? NoModText(a_id) : Joined(off, " ");
		}
		LogChange(a_id, r, why, extra);
		std::scoped_lock l(lastLock);
		last[a_id] = r;
		if (!used) {
			lastWhy[a_id] = why;
		} else {
			lastFine[a_id] = FineText(a_id, *used);
		}
		return r;
	}

	Reading Last(Id a_id)
	{
		std::scoped_lock l(lastLock);
		return a_id < kCount ? last[a_id] : Reading{};
	}

	bool Used(Id a_id)
	{
		return a_id < kBuiltInCount || Extras::Get(a_id) != nullptr;
	}

	bool Installed(Id a_id)
	{
		return a_id < kCount && std::ranges::any_of(sources[a_id], [](const Source& s) { return s.installed; });
	}

	const char* Name(Id a_id)
	{
		constexpr std::array NAMES{ "Hunger", "Cold", "Fatigue", "Stress", "Dirt and Blood", "Encumbrance", "Thirst", "Wetness", "Vitality", "Disease" };
		static_assert(NAMES.size() == kBuiltInCount);
		if (a_id < kBuiltInCount) {
			return NAMES[a_id];
		}
		const auto* extra = Extras::Get(a_id);
		return extra ? extra->name.c_str() : "";
	}

	std::string WhyHidden(Id a_id)
	{
		std::scoped_lock l(lastLock);
		return a_id < kCount ? lastWhy[a_id] : std::string();
	}

	std::string FineMeaning(Id a_id)
	{
		std::scoped_lock l(lastLock);
		return a_id < kCount ? lastFine[a_id] : std::string();
	}

	int FineUpTo(Id a_id)
	{
		constexpr std::array FINE_UP_TO{ 1, 1, 1, 1, 2, 2, 1, 1, 1, 0 };
		static_assert(FINE_UP_TO.size() == kBuiltInCount);
		if (a_id < kBuiltInCount) {
			return FINE_UP_TO[a_id];
		}
		const auto* extra = Extras::Get(a_id);
		return extra ? extra->fineUpTo : 0;
	}
}
