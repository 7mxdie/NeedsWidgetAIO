#include "Settings.h"

#include "Extras.h"

namespace Settings
{
	namespace
	{
		// Where each icon sits in Free if the defaults file says nothing: a row above
		// the bottom right corner. By need id; thirst and wetness (TEST7) go on the left,
		// and extra icons further left again, 14 to a row, then a row above (their own
		// file can say otherwise).
		constexpr std::array<float, Needs::kBuiltInCount> DEFAULT_X{ 76.0f, 80.0f, 84.0f, 88.0f, 92.0f, 96.0f, 72.0f, 68.0f, 64.0f, 60.0f };
		constexpr std::uint32_t                           EXTRAS_PER_ROW = 14;

		float DefaultX(std::uint32_t a_id)
		{
			return a_id < Needs::kBuiltInCount ? DEFAULT_X[a_id] : 56.0f - 4.0f * static_cast<float>((a_id - Needs::kBuiltInCount) % EXTRAS_PER_ROW);
		}

		float DefaultY(std::uint32_t a_id)
		{
			return a_id < Needs::kBuiltInCount ? Icon{}.y : Icon{}.y - 8.0f * static_cast<float>((a_id - Needs::kBuiltInCount) / EXTRAS_PER_ROW);
		}

		// Stage colours, best (0) to worst (5). The worst is a bright red, not a dark
		// one: it is the state that most needs seeing, and dark red sank into the
		// dark frame on the comparison sheet.
		constexpr std::array<std::uint32_t, 6> BONE_TO_RED{ 0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F };
		constexpr std::array<std::uint32_t, 6> GREEN_TO_RED{ 0x6FBF5A, 0xA9C94E, 0xE3C94A, 0xE8A060, 0xD9503F, 0xFF2E1F };

		std::mutex                      lock;
		std::array<Icon, Needs::kCount> current;
		std::array<Icon, Needs::kCount> saved;
		std::array<Icon, Needs::kCount> defaults;
		General                         currentGeneral;
		General                         savedGeneral;
		General                         defaultGeneral;
		Style                           style;
		std::vector<Preset>             presets;
		std::atomic<bool>               dirty{ true };
		float                           anchorX{ 86.0f };  // middle of the default layout: the group scales about it
		float                           anchorY{ 87.0f };
		std::atomic<std::int64_t>       lastEdit{ 0 };     // steady clock, milliseconds

		constexpr std::int64_t EDITING_FOR_MS = 500;  // a page counts as open this long after it last drew

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		// Pinned the first time it's asked (kPostLoad, game folder as the working folder).
		// Worked out on every call, a plugin or dialog that moves the working folder later
		// sent Save somewhere else, and the layout was gone after a restart (fuqdupd, 2.3.0).
		std::filesystem::path PluginDir()
		{
			static const auto dir = std::filesystem::absolute("Data/SKSE/Plugins");
			return dir;
		}

		std::wstring DefaultsPath() { return (PluginDir() / "NeedsWidget.ini").wstring(); }
		std::filesystem::path PresetDir() { return PluginDir() / "NeedsWidget" / "Presets"; }
		std::wstring CustomPath() { return (PluginDir() / "NeedsWidget_Custom.ini").wstring(); }

		std::wstring Wide(std::string_view a_ascii)
		{
			return std::wstring(a_ascii.begin(), a_ascii.end());
		}

		std::wstring ReadString(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key)
		{
			wchar_t buf[1024]{};
			GetPrivateProfileStringW(a_section.c_str(), a_key, L"", buf, 1024, a_file.c_str());
			std::wstring s(buf);
			if (const auto semicolon = s.find(L';'); semicolon != std::wstring::npos) {
				s.erase(semicolon);  // a comment after the value
			}
			while (!s.empty() && iswspace(s.back())) {
				s.pop_back();
			}
			return s;
		}

		float ReadFloat(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key, float a_fallback)
		{
			const auto s = ReadString(a_file, a_section, a_key);
			if (s.empty()) {
				return a_fallback;
			}
			// Not stof: that follows the C locale, which another plugin can switch to one
			// with a decimal comma. Files an older build wrote that way still read.
			std::string ascii;
			for (const wchar_t c : s) {
				ascii.push_back(c == L',' ? '.' : c < 128 ? static_cast<char>(c) : '?');
			}
			float value = a_fallback;
			const auto* first = ascii.data();
			while (first < ascii.data() + ascii.size() && *first == '+') {
				++first;
			}
			const auto [end, ec] = std::from_chars(first, ascii.data() + ascii.size(), value);
			return ec == std::errc() && end != first ? value : a_fallback;
		}

		bool ReadBool(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key, bool a_fallback)
		{
			return ReadFloat(a_file, a_section, a_key, a_fallback ? 1.0f : 0.0f) != 0.0f;
		}

		std::uint32_t ReadColor(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key, std::uint32_t a_fallback)
		{
			const auto s = ReadString(a_file, a_section, a_key);
			if (s.empty()) {
				return a_fallback;
			}
			try {
				return static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)) & 0xFFFFFF;
			} catch (...) {
				return a_fallback;
			}
		}

		std::string ReadName(const std::wstring& a_file, const wchar_t* a_key, const std::string& a_fallback)
		{
			const auto s = ReadString(a_file, L"Style", a_key);
			if (s.empty()) {
				return a_fallback;
			}
			std::string out;
			for (const auto c : s) {
				out += static_cast<char>(c < 128 ? c : '_');
			}
			return out;
		}

		Icon ReadIcon(const std::wstring& a_file, const std::wstring& section, const Icon& a_fallback)
		{
			Icon i;
			i.x = ReadFloat(a_file, section, L"fPosX", a_fallback.x);
			i.y = ReadFloat(a_file, section, L"fPosY", a_fallback.y);
			i.size = ReadFloat(a_file, section, L"fSize", a_fallback.size);
			i.opacity = ReadFloat(a_file, section, L"fOpacity", a_fallback.opacity);
			i.enabled = ReadBool(a_file, section, L"bEnabled", a_fallback.enabled);
			i.showNumber = ReadBool(a_file, section, L"bShowNumber", a_fallback.showNumber);
			i.hideWhenFine = ReadBool(a_file, section, L"bHideWhenFine", a_fallback.hideWhenFine);
			return i;
		}

		Icon ReadIcon(const std::wstring& a_file, Needs::Id a_id, const Icon& a_fallback)
		{
			return Needs::Used(a_id) ? ReadIcon(a_file, Wide(Needs::Name(a_id)), a_fallback) : a_fallback;
		}

		std::string Narrow(const std::wstring& a_ascii)
		{
			std::string out;
			for (const auto c : a_ascii) {
				out += static_cast<char>(c < 128 ? c : '_');
			}
			return out;
		}

		std::wstring Trimmed(std::wstring a_text)
		{
			while (!a_text.empty() && iswspace(a_text.back())) {
				a_text.pop_back();
			}
			while (!a_text.empty() && iswspace(a_text.front())) {
				a_text.erase(a_text.begin());
			}
			return a_text;
		}

		// sOrder = 9,0,6,1,7,2,8,3,4,5 (0 Hunger, 1 Cold, 2 Fatigue, 3 Stress, 4 Dirt
		// and Blood, 5 Encumbrance, 6 Thirst, 7 Wetness, 8 Vitality, 9 Disease), with
		// extra icons by name: 9,Arousal,0,... Their slot number can change as addons
		// come and go; their name can't. Unknown entries and repeats are skipped. An
		// extra icon the list leaves out goes at the start, out at the free end of a
		// row that starts from the right (the default); a built-in need it leaves out
		// goes on the end in the fallback's order, so a preset written before a need
		// was added still loads.
		std::array<int, Needs::kCount> ReadOrder(const std::wstring& a_file, const std::array<int, Needs::kCount>& a_fallback)
		{
			const auto text = ReadString(a_file, L"General", L"sOrder");
			if (text.empty()) {
				return a_fallback;
			}
			std::vector<int>                listed;
			std::array<bool, Needs::kCount> seen{};
			std::wstringstream              in(text);
			std::wstring                    item;
			while (std::getline(in, item, L',')) {
				item = Trimmed(item);
				if (item.empty()) {
					continue;
				}
				int id = -1;
				if (std::iswdigit(item.front())) {
					try {
						id = std::stoi(item);
					} catch (...) {
					}
					if (id >= static_cast<int>(Needs::kBuiltInCount)) {
						id = -1;  // extra icons are never saved by number
					}
				} else if (const auto extra = Extras::Find(Narrow(item))) {
					id = static_cast<int>(*extra);
				}
				if (id >= 0 && !seen[id]) {
					seen[id] = true;
					listed.push_back(id);
				}
			}
			std::array<int, Needs::kCount> order{};
			std::size_t                    n = 0;
			for (const auto id : a_fallback) {
				if (!seen[id] && Needs::IsExtra(static_cast<Needs::Id>(id))) {
					seen[id] = true;
					order[n++] = id;
				}
			}
			for (const auto id : listed) {
				order[n++] = id;
			}
			for (const auto id : a_fallback) {
				if (!seen[id]) {
					seen[id] = true;
					order[n++] = id;
				}
			}
			return order;
		}

		std::wstring OrderText(const std::array<int, Needs::kCount>& a_order)
		{
			std::wstring out;
			for (const auto id : a_order) {
				const auto need = static_cast<Needs::Id>(id);
				if (!Needs::Used(need)) {
					continue;  // an empty extra slot
				}
				if (!out.empty()) {
					out += L',';
				}
				out += Needs::IsExtra(need) ? Wide(Needs::Name(need)) : std::to_wstring(id);
			}
			return out;
		}

		General ReadGeneral(const std::wstring& a_file, const General& a_fallback)
		{
			General g;
			const auto look = static_cast<int>(ReadFloat(a_file, L"General", L"iLook", static_cast<float>(a_fallback.look)));
			g.look = static_cast<Look>(std::clamp(look, 0, 2));
			g.tintArt = ReadBool(a_file, L"General", L"bTintIcons", a_fallback.tintArt);
			const auto colors = static_cast<int>(ReadFloat(a_file, L"General", L"iColorScheme", static_cast<float>(a_fallback.colors)));
			g.colors = static_cast<ColorScheme>(std::clamp(colors, 0, 2));
			g.pulseWorst = ReadBool(a_file, L"General", L"bPulseWorst", a_fallback.pulseWorst);
			g.numbersAbove = ReadBool(a_file, L"General", L"bNumbersAbove", a_fallback.numbersAbove);
			g.numberSize = std::clamp(ReadFloat(a_file, L"General", L"fNumberSize", a_fallback.numberSize), 50.0f, 400.0f);
			g.numberGap = std::clamp(ReadFloat(a_file, L"General", L"fNumberGap", a_fallback.numberGap), -60.0f, 60.0f);
			g.numberAlign = std::clamp(static_cast<int>(ReadFloat(a_file, L"General", L"iNumberAlign", static_cast<float>(a_fallback.numberAlign))), 0, 2);
			g.numberX = std::clamp(ReadFloat(a_file, L"General", L"fNumberX", a_fallback.numberX), -200.0f, 200.0f);
			g.numberTint = ReadBool(a_file, L"General", L"bNumberTint", a_fallback.numberTint);
			g.numberEdge = ReadBool(a_file, L"General", L"bNumberEdge", a_fallback.numberEdge);
			g.numberShows = std::clamp(static_cast<int>(ReadFloat(a_file, L"General", L"iNumberShows", static_cast<float>(a_fallback.numberShows))), 0, 1);
			g.fadeWhenStill = ReadBool(a_file, L"General", L"bFadeWhenStill", a_fallback.fadeWhenStill);
			g.fadeAfter = std::clamp(ReadFloat(a_file, L"General", L"fFadeAfter", a_fallback.fadeAfter), 2.0f, 120.0f);
			g.keepBad = ReadBool(a_file, L"General", L"bKeepBadShown", a_fallback.keepBad);
			g.flashWorse = ReadBool(a_file, L"General", L"bFlashWorse", a_fallback.flashWorse);
			g.hotkey = std::clamp(static_cast<int>(ReadFloat(a_file, L"General", L"iHotkey", static_cast<float>(a_fallback.hotkey))), 0, 255);
			g.hotkeyMode = std::clamp(static_cast<int>(ReadFloat(a_file, L"General", L"iHotkeyMode", static_cast<float>(a_fallback.hotkeyMode))), 0, 1);
			g.groupSize = std::clamp(ReadFloat(a_file, L"General", L"fGroupSize", a_fallback.groupSize), 25.0f, 300.0f);
			g.groupX = std::clamp(ReadFloat(a_file, L"General", L"fGroupMoveX", a_fallback.groupX), -100.0f, 100.0f);
			g.groupY = std::clamp(ReadFloat(a_file, L"General", L"fGroupMoveY", a_fallback.groupY), -100.0f, 100.0f);
			const auto arrangement = static_cast<int>(ReadFloat(a_file, L"General", L"iArrangement", static_cast<float>(a_fallback.arrangement)));
			g.arrangement = static_cast<Arrangement>(std::clamp(arrangement, 0, 2));
			g.fromFarEnd = ReadBool(a_file, L"General", L"bFromFarEnd", a_fallback.fromFarEnd);
			g.lineX = std::clamp(ReadFloat(a_file, L"General", L"fLinePosX", a_fallback.lineX), 0.0f, 100.0f);
			g.lineY = std::clamp(ReadFloat(a_file, L"General", L"fLinePosY", a_fallback.lineY), 0.0f, 100.0f);
			g.spacing = std::clamp(ReadFloat(a_file, L"General", L"fSpacing", a_fallback.spacing), 0.0f, 200.0f);
			g.closeGaps = ReadBool(a_file, L"General", L"bCloseGaps", a_fallback.closeGaps);
			g.perLine = std::clamp(static_cast<int>(ReadFloat(a_file, L"General", L"iPerLine", static_cast<float>(a_fallback.perLine))), 0, static_cast<int>(Needs::kCount));
			g.wrapBefore = ReadBool(a_file, L"General", L"bWrapBefore", a_fallback.wrapBefore);
			g.lineGap = std::clamp(ReadFloat(a_file, L"General", L"fLineGap", a_fallback.lineGap), 0.0f, 200.0f);
			g.order = ReadOrder(a_file, a_fallback.order);
			// TEST5 and earlier kept these under [Style] in the defaults file; a
			// reskin written then still works.
			for (std::size_t k = 0; k < g.customColors.size(); ++k) {
				const auto key = std::format(L"sStage{}Color", k);
				const auto old = ReadColor(a_file, L"Style", key.c_str(), a_fallback.customColors[k]);
				g.customColors[k] = ReadColor(a_file, L"General", key.c_str(), old);
			}
			return g;
		}

		Style ReadStyle(const std::wstring& a_file)
		{
			Style s;
			const Style d;
			const std::wstring section = L"Style";
			s.ringFrame = ReadName(a_file, L"sRingFrame", d.ringFrame);
			s.ringArtSize = ReadFloat(a_file, section, L"fRingArtSize", d.ringArtSize);
			s.ringArtY = ReadFloat(a_file, section, L"fRingArtY", d.ringArtY);
			s.ringRadius = ReadFloat(a_file, section, L"fRingRadius", d.ringRadius);
			s.ringWidth = ReadFloat(a_file, section, L"fRingWidth", d.ringWidth);
			s.ringLabelY = ReadFloat(a_file, section, L"fRingLabelY", d.ringLabelY);
			s.barFrame = ReadName(a_file, L"sBarFrame", d.barFrame);
			s.barArtSize = ReadFloat(a_file, section, L"fBarArtSize", d.barArtSize);
			s.barArtY = ReadFloat(a_file, section, L"fBarArtY", d.barArtY);
			s.barLeft = ReadFloat(a_file, section, L"fBarLeft", d.barLeft);
			s.barTop = ReadFloat(a_file, section, L"fBarTop", d.barTop);
			s.barLength = ReadFloat(a_file, section, L"fBarLength", d.barLength);
			s.barHeight = ReadFloat(a_file, section, L"fBarHeight", d.barHeight);
			s.barLabelY = ReadFloat(a_file, section, L"fBarLabelY", d.barLabelY);
			s.iconArtSize = ReadFloat(a_file, section, L"fIconArtSize", d.iconArtSize);
			s.iconArtY = ReadFloat(a_file, section, L"fIconArtY", d.iconArtY);
			s.iconLabelY = ReadFloat(a_file, section, L"fIconLabelY", d.iconLabelY);
			return s;
		}

		std::wstring Hex(std::uint32_t a_color)
		{
			wchar_t buf[16]{};
			swprintf_s(buf, L"%06X", a_color & 0xFFFFFF);
			return buf;
		}

		std::wstring Number(float a_value)
		{
			return std::format(L"{:.3f}", a_value);  // always a decimal point, whatever the locale
		}

		int writeFailures = 0;  // since the last Save or export started

		// Save builds the whole file here and writes it in one go. Through
		// WritePrivateProfileString, every write said it worked and yet no file was there
		// (2.3.0 and 2.4.0, on Windows and under Proton alike), while a preset export in
		// the same session worked. The custom file is the one read at start-up and then
		// deleted by Save, so something caching the profile functions in game is the
		// likely cause; outside the game the same calls work. This doesn't use them.
		struct PendingFile
		{
			std::wstring                                                                         file;
			std::vector<std::pair<std::wstring, std::vector<std::pair<std::wstring, std::wstring>>>> sections;
		};
		std::optional<PendingFile> pending;

		void Write(const std::wstring& a_file, const std::wstring& a_section, const wchar_t* a_key, const std::wstring& a_value)
		{
			if (pending && pending->file == a_file) {
				auto section = std::ranges::find_if(pending->sections, [&](const auto& s) { return _wcsicmp(s.first.c_str(), a_section.c_str()) == 0; });
				if (section == pending->sections.end()) {
					section = pending->sections.insert(pending->sections.end(), { a_section, {} });
				}
				auto key = std::ranges::find_if(section->second, [&](const auto& k) { return _wcsicmp(k.first.c_str(), a_key) == 0; });
				if (key == section->second.end()) {
					section->second.emplace_back(a_key, a_value);
				} else {
					key->second = a_value;
				}
				return;
			}
			if (!WritePrivateProfileStringW(a_section.c_str(), a_key, a_value.c_str(), a_file.c_str()) && writeFailures++ == 0) {
				logs::warn("could not write {} (Windows error {})", std::filesystem::path(a_file).string(), GetLastError());
			}
		}

		// Plain ANSI text, as WritePrivateProfileString itself writes a new file, so
		// GetPrivateProfileString reads it back the same. Written to a .tmp first and then
		// swapped in, so a failed write never leaves half a file behind.
		bool WritePending()
		{
			std::wstring text;
			for (const auto& [section, keys] : pending->sections) {
				text += std::format(L"[{}]\r\n", section);
				for (const auto& [key, value] : keys) {
					text += std::format(L"{}={}\r\n", key, value);
				}
				text += L"\r\n";
			}
			std::string bytes;
			if (!text.empty()) {
				const int n = WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
				bytes.resize(static_cast<std::size_t>(n));
				WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(), n, nullptr, nullptr);
			}
			const std::filesystem::path target(pending->file);
			auto                        temp = target;
			temp += L".tmp";
			{
				std::ofstream out(temp, std::ios::binary | std::ios::trunc);
				out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
				out.close();
				if (!out) {
					logs::warn("could not write {}", temp.string());
					return false;
				}
			}
			std::error_code ec;
			std::filesystem::rename(temp, target, ec);
			if (ec) {
				logs::warn("could not put {} in place: {}", target.string(), ec.message());
				std::filesystem::remove(temp, ec);
				return false;
			}
			std::ifstream in(target, std::ios::binary);
			const std::string back((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (back != bytes) {
				logs::warn("{} reads back different from what was written ({} of {} bytes)", target.string(), back.size(), bytes.size());
				return false;
			}
			return true;
		}

		void WriteGeneral(const std::wstring& a_file, const General& a_g)
		{
			Write(a_file, L"General", L"iLook", std::to_wstring(static_cast<int>(a_g.look)));
			Write(a_file, L"General", L"bTintIcons", a_g.tintArt ? L"1" : L"0");
			Write(a_file, L"General", L"iColorScheme", std::to_wstring(static_cast<int>(a_g.colors)));
			Write(a_file, L"General", L"bPulseWorst", a_g.pulseWorst ? L"1" : L"0");
			Write(a_file, L"General", L"bNumbersAbove", a_g.numbersAbove ? L"1" : L"0");
			Write(a_file, L"General", L"fNumberSize", Number(a_g.numberSize));
			Write(a_file, L"General", L"fNumberGap", Number(a_g.numberGap));
			Write(a_file, L"General", L"fNumberX", Number(a_g.numberX));
			Write(a_file, L"General", L"iNumberAlign", std::to_wstring(a_g.numberAlign));
			Write(a_file, L"General", L"bNumberTint", a_g.numberTint ? L"1" : L"0");
			Write(a_file, L"General", L"bNumberEdge", a_g.numberEdge ? L"1" : L"0");
			Write(a_file, L"General", L"iNumberShows", std::to_wstring(a_g.numberShows));
			Write(a_file, L"General", L"bFadeWhenStill", a_g.fadeWhenStill ? L"1" : L"0");
			Write(a_file, L"General", L"fFadeAfter", Number(a_g.fadeAfter));
			Write(a_file, L"General", L"bKeepBadShown", a_g.keepBad ? L"1" : L"0");
			Write(a_file, L"General", L"bFlashWorse", a_g.flashWorse ? L"1" : L"0");
			Write(a_file, L"General", L"iHotkey", std::to_wstring(a_g.hotkey));
			Write(a_file, L"General", L"iHotkeyMode", std::to_wstring(a_g.hotkeyMode));
			Write(a_file, L"General", L"fGroupSize", Number(a_g.groupSize));
			Write(a_file, L"General", L"fGroupMoveX", Number(a_g.groupX));
			Write(a_file, L"General", L"fGroupMoveY", Number(a_g.groupY));
			Write(a_file, L"General", L"iArrangement", std::to_wstring(static_cast<int>(a_g.arrangement)));
			Write(a_file, L"General", L"bFromFarEnd", a_g.fromFarEnd ? L"1" : L"0");
			Write(a_file, L"General", L"fLinePosX", Number(a_g.lineX));
			Write(a_file, L"General", L"fLinePosY", Number(a_g.lineY));
			Write(a_file, L"General", L"fSpacing", Number(a_g.spacing));
			Write(a_file, L"General", L"bCloseGaps", a_g.closeGaps ? L"1" : L"0");
			Write(a_file, L"General", L"iPerLine", std::to_wstring(a_g.perLine));
			Write(a_file, L"General", L"bWrapBefore", a_g.wrapBefore ? L"1" : L"0");
			Write(a_file, L"General", L"fLineGap", Number(a_g.lineGap));
			Write(a_file, L"General", L"sOrder", OrderText(a_g.order));
			for (std::size_t k = 0; k < a_g.customColors.size(); ++k) {
				Write(a_file, L"General", std::format(L"sStage{}Color", k).c_str(), Hex(a_g.customColors[k]));
			}
		}

		void WriteIcon(const std::wstring& a_file, Needs::Id a_id, const Icon& a_i)
		{
			const auto section = Wide(Needs::Name(a_id));
			Write(a_file, section, L"fPosX", Number(a_i.x));
			Write(a_file, section, L"fPosY", Number(a_i.y));
			Write(a_file, section, L"fSize", Number(a_i.size));
			Write(a_file, section, L"fOpacity", Number(a_i.opacity));
			Write(a_file, section, L"bEnabled", a_i.enabled ? L"1" : L"0");
			Write(a_file, section, L"bShowNumber", a_i.showNumber ? L"1" : L"0");
			Write(a_file, section, L"bHideWhenFine", a_i.hideWhenFine ? L"1" : L"0");
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

		// Call with the lock held.
		void ScanPresetsLocked()
		{
			presets.clear();
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(PresetDir(), ec)) {
				if (!entry.is_regular_file() || _wcsicmp(entry.path().extension().c_str(), L".ini") != 0) {
					continue;
				}
				const auto file = entry.path().wstring();
				Preset     p;
				p.file = entry.path();
				const auto name = ReadString(file, L"Preset", L"sName");
				p.name = Utf8(name.empty() ? entry.path().stem().wstring() : name);
				p.note = Utf8(ReadString(file, L"Preset", L"sNote"));
				p.general = ReadGeneral(file, defaultGeneral);
				for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
					p.icons[id] = ReadIcon(file, static_cast<Needs::Id>(id), defaults[id]);
				}
				presets.push_back(std::move(p));
			}
			std::ranges::sort(presets, [](const Preset& a, const Preset& b) { return _stricmp(a.name.c_str(), b.name.c_str()) < 0; });
			logs::info("{} preset(s) in {}", presets.size(), PresetDir().string());
		}
	}

	void Load()
	{
		std::scoped_lock l(lock);
		const auto defaultsFile = DefaultsPath();
		const auto customFile = CustomPath();
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			const auto need = static_cast<Needs::Id>(id);
			Icon       fallback;
			fallback.x = DefaultX(id);
			fallback.y = DefaultY(id);
			// An extra icon's own file can set where it starts; NeedsWidget.ini still wins.
			if (const auto* extra = Extras::Get(need)) {
				fallback = ReadIcon(extra->file.wstring(), L"Icon", fallback);
			}
			defaults[id] = ReadIcon(defaultsFile, need, fallback);
			current[id] = ReadIcon(customFile, need, defaults[id]);
			if (!Needs::Used(need)) {
				continue;
			}
			const auto& i = current[id];
			logs::info("{}: x {:.1f} y {:.1f} size {:.0f} opacity {:.0f} shown {} number {} hide-when-fine {}",
				Needs::Name(need), i.x, i.y, i.size, i.opacity, i.enabled, i.showNumber, i.hideWhenFine);
		}
		defaultGeneral = ReadGeneral(defaultsFile, General{});
		currentGeneral = ReadGeneral(customFile, defaultGeneral);
		style = ReadStyle(defaultsFile);
		// The middle of the built-in needs' default layout, so an addon doesn't move
		// where everyone's group scales about.
		anchorX = anchorY = 0.0f;
		for (std::uint32_t id = 0; id < Needs::kBuiltInCount; ++id) {
			anchorX += defaults[id].x / static_cast<float>(Needs::kBuiltInCount);
			anchorY += defaults[id].y / static_cast<float>(Needs::kBuiltInCount);
		}
		saved = current;
		savedGeneral = currentGeneral;
		dirty = true;
		ScanPresetsLocked();
		logs::info("look {} (0 ring, 1 bar, 2 icon only), colour icons {}, colours {} (0 bone to red, 1 green to red, 2 custom), pulse {}",
			static_cast<int>(currentGeneral.look), currentGeneral.tintArt, static_cast<int>(currentGeneral.colors), currentGeneral.pulseWorst);
		if (currentGeneral.colors == ColorScheme::kCustom) {
			const auto& c = currentGeneral.customColors;
			logs::info("custom colours {:06X} {:06X} {:06X} {:06X} {:06X} {:06X}", c[0], c[1], c[2], c[3], c[4], c[5]);
		}
		logs::info("all icons: size {:.0f}, moved {:.1f}, {:.1f}, scaled about {:.1f}, {:.1f}",
			currentGeneral.groupSize, currentGeneral.groupX, currentGeneral.groupY, anchorX, anchorY);
		logs::info("arrangement {} (0 row, 1 column, 2 free), from the far end {}, at {:.1f}, {:.1f}, spacing {:.0f}%, close gaps {}, order {}",
			static_cast<int>(currentGeneral.arrangement), currentGeneral.fromFarEnd, currentGeneral.lineX, currentGeneral.lineY,
			currentGeneral.spacing, currentGeneral.closeGaps, std::filesystem::path(OrderText(currentGeneral.order)).string());
		logs::info("per row or column {} (0 no limit), new ones before {}, gap {:.0f}%",
			currentGeneral.perLine, currentGeneral.wrapBefore, currentGeneral.lineGap);
	}

	bool Save()
	{
		std::scoped_lock l(lock);
		const auto file = CustomPath();
		writeFailures = 0;
		pending.emplace(PendingFile{ file, {} });  // a whole new file, so only what differs is in it
		int sections = 0;
		if (!(currentGeneral == defaultGeneral)) {
			WriteGeneral(file, currentGeneral);
			++sections;
		}
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			if (current[id] == defaults[id] || !Needs::Used(static_cast<Needs::Id>(id))) {
				continue;
			}
			WriteIcon(file, static_cast<Needs::Id>(id), current[id]);
			++sections;
		}
		bool written = true;
		if (sections > 0) {
			written = WritePending();
		} else {
			std::error_code ec;
			std::filesystem::remove(file, ec);  // all defaults: no file at all
			if (ec) {
				logs::warn("could not clear {}: {}", std::filesystem::path(file).string(), ec.message());
				written = false;
			}
		}
		pending.reset();
		if (!written) {
			logs::warn("settings NOT saved to {}", std::filesystem::path(file).string());
			return false;
		}
		saved = current;
		savedGeneral = currentGeneral;
		logs::info("settings saved to {}: {} section(s) differ from the defaults", std::filesystem::path(file).string(), sections);
		return true;
	}

	void ScanPresets()
	{
		std::scoped_lock l(lock);
		ScanPresetsLocked();
	}

	std::vector<Preset> GetPresets()
	{
		std::scoped_lock l(lock);
		return presets;
	}

	void ApplyPreset(const Preset& a_preset)
	{
		std::scoped_lock l(lock);
		currentGeneral = a_preset.general;
		current = a_preset.icons;
		dirty = true;
		logs::info("preset applied: {}", a_preset.name);
	}

	bool IsInUse(const Preset& a_preset)
	{
		std::scoped_lock l(lock);
		return currentGeneral == a_preset.general && current == a_preset.icons;
	}

	bool MatchesDefaults()
	{
		std::scoped_lock l(lock);
		return currentGeneral == defaultGeneral && current == defaults;
	}

	std::filesystem::path ExportPreset()
	{
		std::scoped_lock l(lock);
		std::error_code  ec;
		std::filesystem::create_directories(PresetDir(), ec);
		std::filesystem::path path;
		for (int n = 1; n < 1000; ++n) {
			const auto candidate = PresetDir() / (n == 1 ? std::wstring(L"My settings.ini") : std::format(L"My settings {}.ini", n));
			if (!std::filesystem::exists(candidate, ec)) {
				path = candidate;
				break;
			}
		}
		if (path.empty()) {
			logs::warn("could not find a free preset file name in {}", PresetDir().string());
			return {};
		}
		const auto file = path.wstring();
		writeFailures = 0;
		Write(file, L"Preset", L"sName", path.stem().wstring());
		Write(file, L"Preset", L"sNote", L"Saved from the menu. Add who made it and which UI it is for.");
		// Everything, not only what differs: a preset has to look the same whatever
		// defaults the next person has.
		WriteGeneral(file, currentGeneral);
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			if (Needs::Used(static_cast<Needs::Id>(id))) {
				WriteIcon(file, static_cast<Needs::Id>(id), current[id]);
			}
		}
		if (!std::filesystem::exists(path, ec)) {
			logs::warn("could not write {}", path.string());
			return {};
		}
		ScanPresetsLocked();
		logs::info("settings exported as a preset: {}", path.string());
		return path;
	}

	void ShowAllNumbers(bool a_show)
	{
		std::scoped_lock l(lock);
		for (auto& i : current) {
			i.showNumber = a_show;
		}
		dirty = true;
	}

	void RestoreAllDefaults()
	{
		std::scoped_lock l(lock);
		currentGeneral = defaultGeneral;
		current = defaults;
		dirty = true;
	}

	void RestoreDefaults(Needs::Id a_id)
	{
		std::scoped_lock l(lock);
		if (a_id < Needs::kCount) {
			current[a_id] = defaults[a_id];
			dirty = true;
		}
	}

	void RestoreGeneralDefaults()
	{
		std::scoped_lock l(lock);
		currentGeneral = defaultGeneral;
		dirty = true;
	}

	Icon Get(Needs::Id a_id)
	{
		std::scoped_lock l(lock);
		return a_id < Needs::kCount ? current[a_id] : Icon{};
	}

	void Set(Needs::Id a_id, const Icon& a_icon)
	{
		std::scoped_lock l(lock);
		if (a_id < Needs::kCount && !(current[a_id] == a_icon)) {
			current[a_id] = a_icon;
			dirty = true;
		}
	}

	General GetGeneral()
	{
		std::scoped_lock l(lock);
		return currentGeneral;
	}

	void SetGeneral(const General& a_general)
	{
		std::scoped_lock l(lock);
		if (!(currentGeneral == a_general)) {
			currentGeneral = a_general;
			dirty = true;
		}
	}

	Style GetStyle()
	{
		std::scoped_lock l(lock);
		return style;
	}

	std::array<std::uint32_t, 6> StageColors(ColorScheme a_scheme)
	{
		switch (a_scheme) {
		case ColorScheme::kGreenToRed:
			return GREEN_TO_RED;
		case ColorScheme::kCustom:
			{
				std::scoped_lock l(lock);
				return currentGeneral.customColors;
			}
		default:
			return BONE_TO_RED;
		}
	}

	std::array<std::uint32_t, 6> StageColors(const General& a_general)
	{
		return a_general.colors == ColorScheme::kCustom ? a_general.customColors : StageColors(a_general.colors);
	}

	bool HasUnsavedChanges()
	{
		std::scoped_lock l(lock);
		return !(current == saved) || !(currentGeneral == savedGeneral);
	}

	bool ConsumeDirty()
	{
		return dirty.exchange(false);
	}

	Placed Place(const Icon& a_icon, const General& a_general)
	{
		std::scoped_lock l(lock);
		const float k = a_general.groupSize / 100.0f;
		return { anchorX + (a_icon.x - anchorX) * k + a_general.groupX,
			anchorY + (a_icon.y - anchorY) * k + a_general.groupY,
			a_icon.size * k };
	}

	void MarkEditing()
	{
		lastEdit = NowMs();
	}

	bool IsEditing()
	{
		return NowMs() - lastEdit.load() < EDITING_FOR_MS;
	}
}
