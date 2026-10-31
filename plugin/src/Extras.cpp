#include "Extras.h"

namespace Extras
{
	namespace
	{
		constexpr std::size_t MAX_SOURCES = 8;
		constexpr std::size_t MAX_PAGES = 4;  // SettingsUI has this many page functions for them

		std::array<std::optional<Def>, Needs::kCount - Needs::kBuiltInCount> defs;
		std::vector<std::string>                                          pages;

		std::filesystem::path IconDir()
		{
			return std::filesystem::absolute("Data/SKSE/Plugins/NeedsWidget/Icons");
		}

		std::string Utf8(const std::wstring& a_text)
		{
			if (a_text.empty()) {
				return {};
			}
			const int   n = WideCharToMultiByte(CP_UTF8, 0, a_text.c_str(), static_cast<int>(a_text.size()), nullptr, 0, nullptr, nullptr);
			std::string out(static_cast<std::size_t>(n), '\0');
			WideCharToMultiByte(CP_UTF8, 0, a_text.c_str(), static_cast<int>(a_text.size()), out.data(), n, nullptr, nullptr);
			return out;
		}

		// The value, without a trailing ; comment or spaces at either end.
		std::string Read(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key)
		{
			wchar_t buf[1024]{};
			GetPrivateProfileStringW(a_section.c_str(), a_key, L"", buf, 1024, a_file.c_str());
			std::wstring s(buf);
			if (const auto semicolon = s.find(L';'); semicolon != std::wstring::npos) {
				s.erase(semicolon);
			}
			while (!s.empty() && iswspace(s.back())) {
				s.pop_back();
			}
			while (!s.empty() && iswspace(s.front())) {
				s.erase(s.begin());
			}
			return Utf8(s);
		}

		std::optional<float> ReadFloat(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key)
		{
			const auto s = Read(a_file, a_section, a_key);
			if (s.empty()) {
				return std::nullopt;
			}
			try {
				return std::stof(s);
			} catch (...) {
				logs::warn("extra icon {}: [{}] {} = {} is not a number", std::filesystem::path(a_file).filename().string(),
					Utf8(a_section), Utf8(a_key), s);
				return std::nullopt;
			}
		}

		bool Same(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::ranges::equal(a, b, [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
		}

		std::string Lower(std::string a_text)
		{
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text;
		}

		std::vector<std::string> Split(const std::string& a_text, char a_on)
		{
			std::vector<std::string> out;
			std::string              item;
			std::stringstream        in(a_text);
			while (std::getline(in, item, a_on)) {
				while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
					item.pop_back();
				}
				while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
					item.erase(item.begin());
				}
				if (!item.empty()) {
					out.push_back(item);
				}
			}
			return out;
		}

		// Thresholds where stages 1-5 begin. Left out: even steps of the maximum.
		std::array<float, 5> ReadSteps(const std::wstring& a_file, const std::wstring& a_section, float a_max, bool a_higherIsWorse,
			const std::array<float, 5>* a_fallback)
		{
			constexpr std::array<float, 5> EVEN{ 0.2f, 0.4f, 0.6f, 0.8f, 0.95f };
			std::array<float, 5>           steps{};
			for (std::size_t k = 0; k < steps.size(); ++k) {
				const auto key = std::format(L"fStage{}", k + 1);
				const auto fallback = a_fallback ? (*a_fallback)[k] : (a_higherIsWorse ? EVEN[k] : 1.0f - EVEN[k]) * a_max;
				steps[k] = ReadFloat(a_file, a_section, key.c_str()).value_or(fallback);
			}
			const bool ordered = a_higherIsWorse ? std::ranges::is_sorted(steps) : std::ranges::is_sorted(steps, std::greater<>{});
			if (!ordered) {
				logs::warn("extra icon {}: [{}] fStage1-5 are out of order; sorted", std::filesystem::path(a_file).filename().string(), Utf8(a_section));
				if (a_higherIsWorse) {
					std::ranges::sort(steps);
				} else {
					std::ranges::sort(steps, std::greater<>{});
				}
			}
			return steps;
		}

		std::optional<SourceDef> ReadSource(const std::wstring& a_file, std::size_t a_n, const Def& a_icon, float a_max, bool a_higherIsWorse,
			const std::array<float, 5>& a_steps)
		{
			const auto section = std::format(L"Source{}", a_n);
			const auto where = std::format("extra icon {} ({}), [Source{}]", a_icon.name, a_icon.file.filename().string(), a_n);
			SourceDef  s;
			s.type = Lower(Read(a_file, section, L"sType"));
			if (s.type.empty()) {
				return std::nullopt;
			}
			s.mod = Read(a_file, section, L"sMod");
			s.plugins = Split(Read(a_file, section, L"sPlugin"), '|');
			if (s.mod.empty()) {
				s.mod = s.plugins.empty() ? std::format("source {}", a_n) : s.plugins.front();
			}
			if (const auto id = Read(a_file, section, L"iFormID"); !id.empty()) {
				try {
					s.formID = static_cast<std::uint32_t>(std::stoul(id, nullptr, 16)) & 0xFFFFFF;
				} catch (...) {
					logs::warn("{}: iFormID = {} is not a hex form ID", where, id);
				}
			}
			s.editorID = Read(a_file, section, L"sEditorID");
			s.actorValue = Read(a_file, section, L"sActorValue");
			s.script = Read(a_file, section, L"sScript");
			s.function = Read(a_file, section, L"sFunction");
			s.key = Read(a_file, section, L"sKey");
			s.variable = Read(a_file, section, L"sVariable");
			s.alias = static_cast<int>(ReadFloat(a_file, section, L"iAlias").value_or(-1.0f));
			// "<hex form ID>:<value>, ...", the value 1 when left out.
			const auto idValues = [&](const wchar_t* a_key) {
				std::vector<std::pair<std::uint32_t, float>> out;
				for (const auto& item : Split(Read(a_file, section, a_key), ',')) {
					const auto colon = item.find(':');
					try {
						const auto id = static_cast<std::uint32_t>(std::stoul(item.substr(0, colon), nullptr, 16)) & 0xFFFFFF;
						const float v = colon == std::string::npos ? 1.0f : std::stof(item.substr(colon + 1));
						out.emplace_back(id, v);
					} catch (...) {
						logs::warn("{}: {} item {} is not <hex form ID>:<value>", where, Utf8(a_key), item);
					}
				}
				return out;
			};
			s.spells = idValues(L"sSpells");
			s.wornKeywords = idValues(L"sWornKeywords");
			s.effects = idValues(L"sEffects");
			s.effectKeywords = idValues(L"sEffectKeywords");
			s.keys = Split(s.key, ',');
			s.noActor = ReadFloat(a_file, section, L"bNoActor").value_or(0.0f) != 0.0f;
			s.passPlayer = ReadFloat(a_file, section, L"bPassPlayer").value_or(1.0f) != 0.0f;
			s.onPlayer = ReadFloat(a_file, section, L"bOnPlayer").value_or(0.0f) != 0.0f;
			s.args = Split(Read(a_file, section, L"sArgs"), ',');
			s.maxFunction = Read(a_file, section, L"sMaxFunction");
			s.maxEditorID = Read(a_file, section, L"sMaxEditorID");
			if (const auto id = Read(a_file, section, L"iMaxFormID"); !id.empty()) {
				try {
					s.maxFormID = static_cast<std::uint32_t>(std::stoul(id, nullptr, 16)) & 0xFFFFFF;
				} catch (...) {
					logs::warn("{}: iMaxFormID = {} is not a hex form ID", where, id);
				}
			}
			s.negativeRanks = ReadFloat(a_file, section, L"bNegativeRanks").value_or(0.0f) != 0.0f;
			for (std::size_t k = 0; k < s.stages.size(); ++k) {
				s.stages[k] = Read(a_file, section, std::format(L"sStage{}", k).c_str());
			}
			s.every = std::clamp(ReadFloat(a_file, section, L"fEvery").value_or(1.0f), 0.25f, 60.0f);
			s.noValueBelow = ReadFloat(a_file, section, L"fNoValueBelow");
			s.showValue = ReadFloat(a_file, section, L"bShowValue").value_or(ReadFloat(a_file, L"Icon", L"bShowValue").value_or(0.0f)) != 0.0f;
			// "116:0, 1-33:4": a single number or an inclusive range (negative numbers allowed,
			// so the range dash is the one after the first character).
			for (const auto& item : Split(Read(a_file, section, L"sRemap"), ',')) {
				const auto colon = item.find(':');
				const auto dash = item.find('-', 1);
				try {
					if (colon == std::string::npos) {
						throw std::invalid_argument("no colon");
					}
					const float from = std::stof(item.substr(0, dash < colon ? dash : colon));
					const float to = dash < colon ? std::stof(item.substr(dash + 1, colon - dash - 1)) : from;
					s.remap.push_back({ from, to, std::stof(item.substr(colon + 1)) });
				} catch (...) {
					logs::warn("{}: sRemap item {} is not <from>-<to>:<value> or <number>:<value>", where, item);
				}
			}
			s.switchEditorID = Read(a_file, section, L"sSwitchEditorID");
			if (const auto id = Read(a_file, section, L"iSwitchFormID"); !id.empty()) {
				try {
					s.switchFormID = static_cast<std::uint32_t>(std::stoul(id, nullptr, 16)) & 0xFFFFFF;
				} catch (...) {
					logs::warn("{}: iSwitchFormID = {} is not a hex form ID", where, id);
				}
			}
			s.switchOn = ReadFloat(a_file, section, L"fSwitchOn").value_or(1.0f);
			s.max = ReadFloat(a_file, section, L"fMax").value_or(a_max);
			s.higherIsWorse = ReadFloat(a_file, section, L"bHigherIsWorse").value_or(a_higherIsWorse ? 1.0f : 0.0f) != 0.0f;
			s.steps = ReadSteps(a_file, section, s.max, s.higherIsWorse, s.higherIsWorse == a_higherIsWorse ? &a_steps : nullptr);
			if (s.max <= 0.0f) {
				logs::warn("{}: fMax must be above 0; using 100", where);
				s.max = 100.0f;
			}

			std::string problem;
			if (s.switchFormID != 0 && s.switchEditorID.empty() && s.plugins.empty()) {
				problem = "iSwitchFormID needs sPlugin";
			} else if (s.type == "global") {
				if (s.editorID.empty() && (s.formID == 0 || s.plugins.empty())) {
					problem = "a Global needs sEditorID, or iFormID with sPlugin";
				}
			} else if (s.type == "factionrank") {
				if (s.formID == 0 || s.plugins.empty()) {
					problem = "a FactionRank needs iFormID and sPlugin";
				}
			} else if (s.type == "actorvalue") {
				if (s.actorValue.empty()) {
					problem = "an ActorValue needs sActorValue";
				}
			} else if (s.type == "papyrus") {
				if (s.script.empty() || s.function.empty()) {
					problem = "a Papyrus source needs sScript and sFunction";
				}
			} else if (s.type == "questfunction") {
				if (s.formID == 0 || s.plugins.empty() || s.script.empty() || s.function.empty()) {
					problem = "a QuestFunction needs iFormID (the quest), sPlugin, sScript and sFunction";
				}
			} else if (s.type == "spells") {
				if ((s.spells.empty() && s.wornKeywords.empty()) || s.plugins.empty()) {
					problem = "a Spells source needs sSpells or sWornKeywords, and sPlugin";
				}
			} else if (s.type == "magiceffect") {
				if ((s.effects.empty() && s.effectKeywords.empty()) || s.plugins.empty()) {
					problem = "a MagicEffect source needs sEffects or sEffectKeywords, and sPlugin";
				}
			} else if (s.type == "storagefloat") {
				if (s.keys.empty()) {
					problem = "a StorageFloat needs sKey";
				}
			} else if (s.type == "scriptvariable") {
				if ((s.formID == 0 && s.editorID.empty()) || s.plugins.empty() || s.script.empty() || s.variable.empty()) {
					problem = "a ScriptVariable needs iFormID or sEditorID (the quest), sPlugin, sScript and sVariable";
				}
			} else if (s.type == "slove") {
				if (s.plugins.empty()) {
					s.plugins = { "SLOVE.esp" };
				}
			} else if (s.type == "minineeds") {
				if (s.plugins.empty()) {
					s.plugins = { "MiniNeeds.esp" };
				}
				if (!Same(s.variable, "Piss") && !Same(s.variable, "Poop")) {
					problem = "a MiniNeeds source needs sVariable = Piss or Poop";
				}
			} else if (s.type == "wounds") {
				if (s.plugins.empty()) {
					s.plugins = { "Wounds.esp" };
				}
			} else if (s.type == "ocum") {
				if (s.plugins.empty()) {
					s.plugins = { "OCum.esp" };
				}
			} else {
				problem = std::format("sType {} is not one this build knows (Global, FactionRank, ActorValue, Papyrus, QuestFunction, StorageFloat, Spells, MagicEffect, ScriptVariable, Wounds, SLOVE, MiniNeeds, OCum)", s.type);
			}
			if (problem.empty() && s.maxFormID != 0 && s.maxEditorID.empty() && s.plugins.empty()) {
				problem = "iMaxFormID needs sPlugin";
			}
			if (!problem.empty()) {
				logs::warn("{} skipped: {}", where, problem);
				return std::nullopt;
			}
			return s;
		}

		bool Reserved(std::string_view a_name)
		{
			for (std::uint32_t id = 0; id < Needs::kBuiltInCount; ++id) {
				if (Same(a_name, Needs::Name(static_cast<Needs::Id>(id)))) {
					return true;
				}
			}
			return Same(a_name, "General") || Same(a_name, "Style") || Same(a_name, "Preset");
		}

		std::optional<Def> ReadIcon(const std::filesystem::path& a_path)
		{
			const auto file = a_path.wstring();
			const auto where = std::format("extra icon file {}", a_path.filename().string());
			Def        d;
			d.file = a_path;
			d.name = Read(file, L"Icon", L"sName");
			if (d.name.empty()) {
				logs::warn("{} skipped: [Icon] has no sName", where);
				return std::nullopt;
			}
			// It becomes an ini section and an sOrder entry, both read back as plain ASCII.
			const bool plain = std::ranges::all_of(d.name, [](char c) { return c >= 0x20 && c < 0x7F; });
			if (!plain || d.name.find_first_of(",[]=;") != std::string::npos || d.name.size() > 40) {
				logs::warn("{} skipped: sName {} must be plain ASCII text (no accents), without , [ ] = or ;, up to 40 long", where, d.name);
				return std::nullopt;
			}
			if (std::isdigit(static_cast<unsigned char>(d.name.front()))) {
				logs::warn("{} skipped: sName {} can't start with a number (sOrder reads that as a built-in need)", where, d.name);
				return std::nullopt;
			}
			if (Reserved(d.name)) {
				logs::warn("{} skipped: sName {} is taken by the main mod", where, d.name);
				return std::nullopt;
			}

			d.art = Read(file, L"Icon", L"sArt");
			if (d.art.empty()) {
				d.art = Lower(d.name);
			}
			for (auto& c : d.art) {
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
					c = '_';
				}
			}
			d.page = Read(file, L"Icon", L"sPage");
			std::ranges::replace(d.page, '/', '-');  // SKSE Menu Framework reads / as a sub-menu
			for (std::size_t k = 0; k < d.stages.size(); ++k) {
				d.stages[k] = Read(file, L"Icon", std::format(L"sStage{}", k).c_str());
				if (d.stages[k].empty()) {
					d.stages[k] = std::format("Stage {}", k);
				}
			}
			d.fineUpTo = std::clamp(static_cast<int>(ReadFloat(file, L"Icon", L"iFineUpTo").value_or(1.0f)), 0, 5);
			d.fine = Read(file, L"Icon", L"sFine");
			d.hideFrom = std::clamp(static_cast<int>(ReadFloat(file, L"Icon", L"iHideFrom").value_or(-1.0f)), -1, 5);
			d.hideFromText = Read(file, L"Icon", L"sHideFrom");
			if (d.hideFromText.empty()) {
				d.hideFromText = "Hide at the worst stages";
			}

			const float max = ReadFloat(file, L"Icon", L"fMax").value_or(100.0f);
			const bool  higherIsWorse = ReadFloat(file, L"Icon", L"bHigherIsWorse").value_or(1.0f) != 0.0f;
			const auto  steps = ReadSteps(file, L"Icon", max > 0.0f ? max : 100.0f, higherIsWorse, nullptr);
			for (std::size_t n = 1; n <= MAX_SOURCES; ++n) {
				if (auto s = ReadSource(file, n, d, max > 0.0f ? max : 100.0f, higherIsWorse, steps)) {
					d.sources.push_back(std::move(*s));
				}
			}
			if (d.sources.empty()) {
				logs::warn("{} skipped: no usable [Source1] ... [Source{}]", where, MAX_SOURCES);
				return std::nullopt;
			}
			return d;
		}
	}

	void Scan()
	{
		std::vector<std::filesystem::path> files;
		std::error_code                    ec;
		for (const auto& entry : std::filesystem::directory_iterator(IconDir(), ec)) {
			if (entry.is_regular_file() && _wcsicmp(entry.path().extension().c_str(), L".ini") == 0) {
				files.push_back(entry.path());
			}
		}
		std::ranges::sort(files, [](const auto& a, const auto& b) { return _wcsicmp(a.filename().c_str(), b.filename().c_str()) < 0; });

		std::size_t used = 0;
		for (const auto& path : files) {
			auto d = ReadIcon(path);
			if (!d) {
				continue;
			}
			if (Find(d->name)) {
				logs::warn("extra icon file {} skipped: another file already has the name {}", path.filename().string(), d->name);
				continue;
			}
			if (used == defs.size()) {
				logs::warn("extra icon file {} skipped: only {} extra icons fit", path.filename().string(), defs.size());
				continue;
			}
			if (!d->page.empty() && std::ranges::find_if(pages, [&](const auto& p) { return Same(p, d->page); }) == pages.end()) {
				if (pages.size() == MAX_PAGES) {
					logs::warn("extra icon {}: only {} extra pages fit, so it goes on the Icons page", d->name, MAX_PAGES);
					d->page.clear();
				} else {
					pages.push_back(d->page);
				}
			}
			std::vector<std::string> kinds;
			for (const auto& s : d->sources) {
				kinds.push_back(std::format("{} ({})", s.mod, s.type));
			}
			logs::info("extra icon {} from {}: slot {}, art {}.swf, page {}, sources {}", d->name, path.filename().string(),
				Needs::kBuiltInCount + used, d->art, d->page.empty() ? "Icons" : d->page, [&] {
					std::string out;
					for (const auto& k : kinds) {
						out += out.empty() ? k : ", then " + k;
					}
					return out;
				}());
			defs[used++] = std::move(*d);
		}
		logs::info("{} extra icon(s) in {}", used, IconDir().string());
	}

	const Def* Get(Needs::Id a_id)
	{
		if (!Needs::IsExtra(a_id)) {
			return nullptr;
		}
		const auto& d = defs[a_id - Needs::kBuiltInCount];
		return d ? &*d : nullptr;
	}

	std::optional<Needs::Id> Find(std::string_view a_name)
	{
		for (std::size_t k = 0; k < defs.size(); ++k) {
			if (defs[k] && Same(defs[k]->name, a_name)) {
				return static_cast<Needs::Id>(Needs::kBuiltInCount + k);
			}
		}
		return std::nullopt;
	}

	std::vector<std::string> Pages()
	{
		return pages;
	}
}
