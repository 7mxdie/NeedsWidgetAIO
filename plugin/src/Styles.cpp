#include "Styles.h"

namespace Styles
{
	namespace
	{
		std::mutex         lock;
		std::vector<Style> styles;

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

		// The value with spaces trimmed. A ; starts a comment, except in sGeometry, which
		// has none (its lists are split by |).
		std::string Read(const std::wstring& a_file, const wchar_t* a_key, bool a_comments = true)
		{
			std::vector<wchar_t> buf(16384);
			GetPrivateProfileStringW(L"Style", a_key, L"", buf.data(), static_cast<DWORD>(buf.size()), a_file.c_str());
			std::wstring s(buf.data());
			if (const auto semicolon = s.find(L';'); a_comments && semicolon != std::wstring::npos) {
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

		float Number(const std::wstring& a_file, const wchar_t* a_key, float a_fallback)
		{
			const auto s = Read(a_file, a_key);
			try {
				return s.empty() ? a_fallback : std::stof(s);
			} catch (...) {
				logs::warn("style {}: {} = {} is not a number", std::filesystem::path(a_file).filename().string(), Utf8(a_key), s);
				return a_fallback;
			}
		}

		std::optional<std::uint32_t> Hex(const std::string& a_text)
		{
			if (a_text.size() != 6 || !std::ranges::all_of(a_text, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; })) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::stoul(a_text, nullptr, 16));
		}

		std::string Lower(std::string a_text)
		{
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text;
		}
	}

	void Scan()
	{
		std::vector<Style> found;
		const auto         dir = std::filesystem::absolute("Data/SKSE/Plugins/NeedsWidget/Styles");
		std::error_code    ec;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
			if (!entry.is_regular_file() || Lower(entry.path().extension().string()) != ".ini") {
				continue;
			}
			const auto file = entry.path().wstring();
			Style      s;
			s.key = Utf8(entry.path().stem().wstring());
			s.name = Read(file, L"sName");
			if (s.name.empty()) {
				s.name = s.key;
			}
			s.note = Read(file, L"sNote");
			s.frame = Read(file, L"sFrame");
			s.meter = Lower(Read(file, L"sMeter"));
			static constexpr std::array<std::string_view, 7> KINDS{ "ring", "trace", "bar", "steps", "level", "glyph", "none" };
			if (std::ranges::find(KINDS, s.meter) == KINDS.end()) {
				logs::warn("style {}: sMeter = {} is not one of ring, trace, bar, steps, level, glyph, none; left out", s.key, s.meter);
				continue;
			}
			s.width = Number(file, L"fWidth", s.width);
			s.geometry = Read(file, L"sGeometry", false);
			s.by = Lower(Read(file, L"sBy"));
			s.alpha = static_cast<int>(Number(file, L"iAlpha", static_cast<float>(s.alpha)));
			s.artSize = Number(file, L"fArtSize", s.artSize);
			s.artY = Number(file, L"fArtY", s.artY);
			const auto glyph = Read(file, L"sGlyph");
			if (const auto rgb = Hex(glyph)) {
				s.glyph = *rgb;
			} else if (!glyph.empty() && Lower(glyph) != "stage") {
				logs::warn("style {}: sGlyph = {} is neither stage nor RRGGBB; using the stage colour", s.key, glyph);
			}
			const auto colors = Read(file, L"sColors");
			if (!colors.empty()) {
				std::array<std::uint32_t, 6> c{};
				std::size_t                  n = 0;
				std::stringstream            in(colors);
				std::string                  item;
				while (std::getline(in, item, ',') && n < 6) {
					while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
						item.erase(item.begin());
					}
					while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
						item.pop_back();
					}
					const auto rgb = Hex(item);
					if (!rgb) {
						break;
					}
					c[n++] = *rgb;
				}
				if (n == 6) {
					s.colors = c;
				} else {
					logs::warn("style {}: sColors needs six RRGGBB colours, best to worst; using bone to red", s.key);
				}
			}
			s.ownColors = Number(file, L"bOwnColors", 0.0f) != 0.0f;
			s.tintFrame = Number(file, L"bTintFrame", 0.0f) != 0.0f;
			s.top = Number(file, L"fTop", s.top);
			s.bottom = Number(file, L"fBottom", s.bottom);
			{
				std::stringstream in(Read(file, L"sDetect"));
				std::string       item;
				while (std::getline(in, item, '|')) {
					while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) {
						item.pop_back();
					}
					while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) {
						item.erase(item.begin());
					}
					if (!item.empty()) {
						s.detect.push_back(item);
					}
				}
			}
			found.push_back(std::move(s));
		}
		// A to Z by name (Tom, TEST27), whatever the file names.
		std::ranges::sort(found, [](const Style& a, const Style& b) { return Lower(a.name) < Lower(b.name); });
		logs::info("{} icon styles in {}", found.size(), dir.string());
		std::scoped_lock l(lock);
		styles = std::move(found);
	}

	std::vector<Style> List()
	{
		std::scoped_lock l(lock);
		return styles;
	}

	std::optional<Style> Detected()
	{
		for (const auto& s : List()) {
			for (const auto& file : s.detect) {
				std::error_code ec;
				if (std::filesystem::exists(std::filesystem::path("Data") / std::filesystem::path(std::u8string(file.begin(), file.end())), ec)) {
					return s;
				}
			}
		}
		return std::nullopt;
	}

	std::optional<Style> Find(std::string_view a_key)
	{
		if (a_key.empty()) {
			return std::nullopt;
		}
		std::scoped_lock l(lock);
		const auto       key = Lower(std::string(a_key));
		for (const auto& s : styles) {
			if (Lower(s.key) == key) {
				return s;
			}
		}
		return std::nullopt;
	}
}
