#pragma once

// FamilyUI.h: one look for the SKSE Menu Framework pages of Tom's mods (Plan or Perish, Arkay's Mercy, Needs Widget,
// Vel'dun, GTS Small Fixes). Header-only. The master copy is Plan or Perish's plugin/shared/FamilyUI.h: copy it into
// each project that uses it, and change it there first.
//
// Include it ONLY from the one file that includes SKSEMenuFramework.h, and after that header: everything here calls
// ImGui through it. Every ImGui function used here must exist in the installed SKSE Menu Framework (it looks them up
// by name as they are called): run Plan or Perish's check_smf_exports.py on the built dll.
//
// Use:
//     FamilyUI::Use({ FamilyUI::kAmethyst, "Arkay's Mercy", "0.9.1", FamilyUI::kIconHeart });   // once, at Register
//     void __stdcall Render() {
//         FamilyUI::Page page;                      // pushes the accent; pops it when it goes out of scope
//         FamilyUI::Title("Settings", "What this page holds.");
//         ...
//         FamilyUI::Footer(on);
//     }
//
// The mod's accent is for titles, headings and the choice in use. The five meaning colours are the same in every mod:
// blue = you changed it, green = saved or fine, red = switched off, orange = a warning, grey = notes.

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace FamilyUI
{
	// ---- colours ----
	inline constexpr ImVec4 kChanged{ 140 / 255.0f, 201 / 255.0f, 250 / 255.0f, 1.0f };
	inline constexpr ImVec4 kGood{ 143 / 255.0f, 212 / 255.0f, 143 / 255.0f, 1.0f };
	inline constexpr ImVec4 kOff{ 242 / 255.0f, 97 / 255.0f, 97 / 255.0f, 1.0f };
	inline constexpr ImVec4 kWarning{ 230 / 255.0f, 140 / 255.0f, 89 / 255.0f, 1.0f };
	inline constexpr ImVec4 kDim{ 168 / 255.0f, 168 / 255.0f, 178 / 255.0f, 1.0f };

	// each mod's accent
	inline constexpr ImVec4 kBlood{ 190 / 255.0f, 40 / 255.0f, 50 / 255.0f, 1.0f };       // Plan or Perish (deep blood, Tom 23 Nov)
	inline constexpr ImVec4 kGold{ 240 / 255.0f, 194 / 255.0f, 102 / 255.0f, 1.0f };      // Vel'dun (its own gold)
	inline constexpr ImVec4 kAmethyst{ 186 / 255.0f, 160 / 255.0f, 242 / 255.0f, 1.0f };  // Arkay's Mercy
	inline constexpr ImVec4 kTeal{ 92 / 255.0f, 208 / 255.0f, 192 / 255.0f, 1.0f };       // Needs Widget
	inline constexpr ImVec4 kRose{ 232 / 255.0f, 140 / 255.0f, 196 / 255.0f, 1.0f };      // spare
	inline constexpr ImVec4 kSteel{ 198 / 255.0f, 212 / 255.0f, 232 / 255.0f, 1.0f };     // GTS Small Fixes

	// FontAwesome (solid) glyphs for beside the title, as UTF-8. All of them are in FontAwesome 4, 5 and 6.
	inline constexpr const char* kIconHeart = "\xEF\x80\x84";  // U+F004
	inline constexpr const char* kIconCog = "\xEF\x80\x93";    // U+F013
	inline constexpr const char* kIconStar = "\xEF\x80\x85";   // U+F005
	inline constexpr const char* kIconPulse = "\xEF\x88\x9E";  // U+F21E, a heart with a pulse line

	// What a mod sets once.
	struct Look
	{
		ImVec4      accent{ kGold };
		const char* mod{ "" };        // "Arkay's Mercy": the footer, and the pages' ID scope
		const char* version{ "" };    // "0.9.1": the footer
		const char* icon{ nullptr };  // a FontAwesome solid glyph beside every page title, or none
	};

	namespace detail
	{
		inline Look look{};
	}

	inline void          Use(const Look& a_look) { detail::look = a_look; }
	inline const Look&   Current() { return detail::look; }
	inline const ImVec4& Accent() { return detail::look.accent; }

	inline ImVec4 Fade(ImVec4 a_colour, float a_alpha)
	{
		a_colour.w = a_alpha;
		return a_colour;
	}

	// ---- the page wrapper ----
	// The accent on ImGui's own tick, slider, heading and tab colours for as long as it lives, so plain Checkbox,
	// SliderFloat, CollapsingHeader and tabs match the page. All popped again at the end: nothing leaks into the
	// framework's window or another mod's page.
	class Page
	{
	public:
		Page()
		{
			const auto& a = Accent();
			ImGui::PushID(detail::look.mod);
			ImGui::PushStyleColor(ImGuiCol_CheckMark, a);
			ImGui::PushStyleColor(ImGuiCol_SliderGrab, Fade(a, 0.75f));
			ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, a);
			ImGui::PushStyleColor(ImGuiCol_Header, Fade(a, 0.16f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Fade(a, 0.28f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, Fade(a, 0.36f));
			ImGui::PushStyleColor(ImGuiCol_Tab, Fade(a, 0.10f));
			ImGui::PushStyleColor(ImGuiCol_TabHovered, Fade(a, 0.36f));
			ImGui::PushStyleColor(ImGuiCol_TabActive, Fade(a, 0.28f));
		}
		~Page()
		{
			ImGui::PopStyleColor(kPushed);
			ImGui::PopID();
		}
		Page(const Page&) = delete;
		Page& operator=(const Page&) = delete;

	private:
		static constexpr int kPushed = 9;
	};

	// A heading bar (CollapsingHeader) in the accent, or red for a part that is switched off.
	class HeaderLook
	{
	public:
		explicit HeaderLook(bool a_off = false)
		{
			const auto& c = a_off ? kOff : Accent();
			ImGui::PushStyleColor(ImGuiCol_Header, Fade(c, 0.16f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Fade(c, 0.28f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, Fade(c, 0.36f));
		}
		~HeaderLook() { ImGui::PopStyleColor(3); }
		HeaderLook(const HeaderLook&) = delete;
		HeaderLook& operator=(const HeaderLook&) = delete;
	};

	// ---- text ----
	inline void Dim(const char* a_text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kDim);
		ImGui::TextWrapped("%s", a_text);
		ImGui::PopStyleColor();
	}

	inline void Wrapped(const ImVec4& a_colour, const char* a_text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, a_colour);
		ImGui::TextWrapped("%s", a_text);
		ImGui::PopStyleColor();
	}

	// A section's title inside a page, in the accent colour.
	inline void Section(const char* a_label)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, Accent());
		ImGui::SeparatorText(a_label);
		ImGui::PopStyleColor();
	}

	// A plain folding heading in the accent.
	inline bool Folder(const char* a_label, bool a_open = false)
	{
		HeaderLook look;
		return ImGui::CollapsingHeader(a_label, a_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
	}

	// A line through the item just drawn (a switched-off line, with its text in kOff). One-line items only.
	inline void Strike()
	{
		ImVec2 min{};
		ImVec2 max{};
		ImGui::GetItemRectMin(&min);
		ImGui::GetItemRectMax(&max);
		if (max.y - min.y > ImGui::GetTextLineHeight() * 1.5f) {
			return;
		}
		const float y = std::round((min.y + max.y) * 0.5f);
		ImGui::ImDrawListManager::AddLine(ImGui::GetWindowDrawList(), ImVec2(min.x, y), ImVec2(max.x, y), ImGui::GetColorU32(kOff), 1.5f);
	}

	// ---- tooltips that wrap: the title in the accent, then lines. Call after the item they belong to. ----
	inline bool TipBegin(const char* a_title)
	{
		if (!ImGui::BeginTooltip()) {
			return false;
		}
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
		if (a_title && *a_title) {
			ImGui::TextColored(Accent(), "%s", a_title);
		}
		return true;
	}

	inline void TipEnd()
	{
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	inline void Tip(const char* a_title, const char* a_text)
	{
		if (a_text && *a_text && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && TipBegin(a_title)) {
			ImGui::TextUnformatted(a_text);
			TipEnd();
		}
	}

	// ---- tags ----
	// A small coloured tag: a preset, a state, what is changed. Not a button, it only looks like one.
	inline void Chip(const std::string& a_text, const ImVec4& a_colour)
	{
		const auto back = Fade(a_colour, 0.18f);
		ImGui::PushStyleColor(ImGuiCol_Button, back);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, back);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, back);
		ImGui::PushStyleColor(ImGuiCol_Text, a_colour);
		ImGui::SmallButton(a_text.c_str());
		ImGui::PopStyleColor(4);
	}

	struct Tag
	{
		std::string text;
		ImVec4      colour{ kDim };
		const char* tipTitle{ nullptr };  // with tip: shown on hover
		const char* tip{ nullptr };
	};

	// Tags side by side, onto the next line when the row is full.
	inline void TagRow(std::span<const Tag> a_tags)
	{
		ImVec2 avail{};
		ImGui::GetContentRegionAvail(&avail);
		const auto* style = ImGui::GetStyle();
		const float right = ImGui::GetCursorPosX() + avail.x;
		ImGui::PushID("tags");
		int n = 0;
		for (const auto& tag : a_tags) {
			if (tag.text.empty()) {
				continue;
			}
			if (n > 0) {
				ImVec2 size{};
				ImGui::CalcTextSize(&size, tag.text.c_str(), nullptr, true, -1.0f);
				ImGui::SameLine();
				if (ImGui::GetCursorPosX() + size.x + style->FramePadding.x * 2.0f > right) {
					ImGui::NewLine();
				}
			}
			ImGui::PushID(n++);
			Chip(tag.text, tag.colour);
			ImGui::PopID();
			if (tag.tip) {
				Tip(tag.tipTitle ? tag.tipTitle : tag.text.c_str(), tag.tip);
			}
		}
		ImGui::PopID();
	}

	// ---- the top of a page: its name in large accent letters (the mod's icon before it) over a rule, then a line on
	// what the page holds ----
	inline void Title(const char* a_title, const char* a_about = nullptr)
	{
		const auto& a = Accent();
		const float size = std::round(ImGui::GetFontSize() * 1.45f);
		ImVec2      pos{};
		ImVec2      avail{};
		ImGui::GetCursorScreenPos(&pos);
		ImGui::GetContentRegionAvail(&avail);
		auto* draw = ImGui::GetWindowDrawList();
		float x = pos.x;
		if (const char* icon = detail::look.icon; icon && *icon) {
			FontAwesome::PushSolid();
			auto*  font = ImGui::GetFont();
			ImVec2 one{};
			ImGui::CalcTextSize(&one, icon, nullptr, false, -1.0f);
			FontAwesome::Pop();
			const float scale = size / (std::max)(1.0f, ImGui::GetFontSize());
			ImGui::ImDrawListManager::AddText_FontPtr(draw, font, size * 0.82f, ImVec2(x, pos.y + size * 0.12f), ImGui::GetColorU32(a), icon,
				nullptr, 0.0f, nullptr);
			x += one.x * scale * 0.82f + size * 0.40f;
		}
		ImGui::ImDrawListManager::AddText_FontPtr(draw, ImGui::GetFont(), size, ImVec2(x, pos.y), ImGui::GetColorU32(a), a_title, nullptr,
			0.0f, nullptr);
		const float y = pos.y + size + 4.0f;
		ImGui::ImDrawListManager::AddRectFilled(draw, ImVec2(pos.x, y), ImVec2(pos.x + avail.x, y + 2.0f), ImGui::GetColorU32(Fade(a, 0.75f)),
			0.0f, 0);
		ImGui::Dummy(ImVec2(avail.x, size + 12.0f));
		if (a_about && *a_about) {
			Dim(a_about);
		}
	}

	// ---- buttons ----
	// One of a few choices: the one in use is the accent, with dark text on a light accent and light on a dark one. a_width 0: as wide as its label.
	inline bool Choice(const char* a_label, bool a_inUse, float a_width = 0.0f)
	{
		if (a_inUse) {
			const auto& a = Accent();
			ImGui::PushStyleColor(ImGuiCol_Button, Fade(a, 0.85f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, a);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, a);
			const bool light = 0.299f * a.x + 0.587f * a.y + 0.114f * a.z > 0.5f;
			ImGui::PushStyleColor(ImGuiCol_Text, light ? ImVec4{ 0.08f, 0.07f, 0.09f, 1.0f } : ImVec4{ 0.97f, 0.94f, 0.92f, 1.0f });
		}
		const bool pressed = ImGui::Button(a_label, ImVec2(a_width, ImGui::GetFrameHeight() * 1.5f));
		if (a_inUse) {
			ImGui::PopStyleColor(4);
		}
		return pressed;
	}

	// A button that wants pressing (Save with unsaved changes): tinted with the accent while a_wanted.
	inline bool AccentButton(const char* a_label, bool a_wanted = true)
	{
		if (a_wanted) {
			const auto& a = Accent();
			ImGui::PushStyleColor(ImGuiCol_Button, Fade(a, 0.55f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Fade(a, 0.75f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, Fade(a, 0.90f));
		}
		const bool pressed = ImGui::Button(a_label);
		if (a_wanted) {
			ImGui::PopStyleColor(3);
		}
		return pressed;
	}

	// ---- search ----
	inline std::string Lower(std::string_view a_text)
	{
		std::string out(a_text);
		std::ranges::transform(out, out.begin(), [](unsigned char a_c) { return static_cast<char>(std::tolower(a_c)); });
		return out;
	}

	// One per mod (a static in its page file): the box's text, and the tidied word to match with.
	struct Search
	{
		char        text[64]{};
		std::string needle;  // lower case, trimmed; set by SearchBox each frame

		bool Active() const { return !needle.empty(); }
		// Whether a_text holds the word (anything matches while the box is empty).
		bool Matches(std::string_view a_text) const { return needle.empty() || Lower(a_text).find(needle) != std::string::npos; }
		void Clear()
		{
			text[0] = '\0';
			needle.clear();
		}
	};

	// The search box across the page with Clear beside it. True while there is a word to match.
	inline bool SearchBox(Search& a_search, const char* a_hint)
	{
		const auto* style = ImGui::GetStyle();
		ImVec2      avail{};
		ImVec2      clear{};
		ImGui::GetContentRegionAvail(&avail);
		ImGui::CalcTextSize(&clear, "Clear", nullptr, true, -1.0f);
		const float button = clear.x + style->FramePadding.x * 2.0f;
		ImGui::PushID("search");
		ImGui::SetNextItemWidth((std::max)(ImGui::GetFontSize() * 8.0f, avail.x - button - style->ItemSpacing.x));
		ImGui::InputTextWithHint("##box", a_hint, a_search.text, sizeof a_search.text);
		ImGui::SameLine();
		ImGui::BeginDisabled(a_search.text[0] == '\0');
		if (ImGui::Button("Clear")) {
			a_search.text[0] = '\0';
		}
		ImGui::EndDisabled();
		ImGui::PopID();
		a_search.needle = Lower(a_search.text);
		std::erase_if(a_search.needle, [](char a_c) { return a_c == '\r' || a_c == '\n'; });
		while (!a_search.needle.empty() && a_search.needle.back() == ' ') {
			a_search.needle.pop_back();
		}
		while (!a_search.needle.empty() && a_search.needle.front() == ' ') {
			a_search.needle.erase(a_search.needle.begin());
		}
		return a_search.Active();
	}

	// The line under the box while searching: what was found, or that nothing was. a_what: "setting", "line".
	inline void SearchResult(std::size_t a_found, std::size_t a_groups, const char* a_what = "setting", const char* a_tail = "")
	{
		if (a_found == 0) {
			ImGui::TextColored(kWarning, "%s", "Nothing matches. Try one short word.");
			return;
		}
		ImGui::TextColored(kDim, "%s",
			std::format("{} {}{} in {} group{}{}", a_found, a_what, a_found == 1 ? "" : "s", a_groups, a_groups == 1 ? "" : "s", a_tail).c_str());
	}

	// ---- Jump to ----
	struct JumpItem
	{
		std::string label;
		bool        off{ false };  // shown red with "(off)"
	};

	// A drop-down of the page's groups, "Open every group" and "Close every group" on top. Returns the index picked
	// or -1; a_foldAll (if given) is set to 1 or -1 when one of the top two is picked. Use both on the NEXT frame.
	inline int JumpTo(std::span<const JumpItem> a_items, int* a_foldAll = nullptr)
	{
		int picked = -1;
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
		if (ImGui::BeginCombo("##jump", std::format("Jump to: {} groups", a_items.size()).c_str())) {
			if (a_foldAll) {
				if (ImGui::Selectable("Open every group", false)) {
					*a_foldAll = 1;
				}
				if (ImGui::Selectable("Close every group", false)) {
					*a_foldAll = -1;
				}
				ImGui::Separator();
			}
			for (std::size_t i = 0; i < a_items.size(); ++i) {
				const auto& item = a_items[i];
				if (item.off) {
					ImGui::PushStyleColor(ImGuiCol_Text, kOff);
				}
				if (ImGui::Selectable(std::format("{}{}###jump{}", item.label, item.off ? " (off)" : "", i).c_str(), false)) {
					picked = static_cast<int>(i);
				}
				if (item.off) {
					ImGui::PopStyleColor();
				}
			}
			ImGui::EndCombo();
		}
		return picked;
	}

	// ---- a group's bar ----
	// "3 settings" / "1 line": the usual badge at the bar's right end.
	inline std::string CountBadge(std::size_t a_count, const char* a_what = "setting")
	{
		return std::format("{} {}{}", a_count, a_what, a_count == 1 ? "" : "s");
	}

	struct Group
	{
		const char* label{ "" };
		std::string badge;                  // at the bar's right end: a count, "2 changed", "OFF"
		ImVec4      badgeColour{ kDim };
		bool*       on{ nullptr };          // the group's own switch before the bar, or none
		bool        locked{ false };        // the switch can't be changed
		const char* switchTip{ nullptr };   // hovering the switch
		int         fold{ 0 };              // this frame: 1 opens it, -1 shuts it
		bool        scroll{ false };        // the page scrolls to it
		bool        openFirst{ false };     // open the first time it is ever shown
	};

	struct GroupState
	{
		bool open{ false };
		bool toggled{ false };  // the switch was pressed: *on holds the new value
		bool hovered{ false };  // the bar is hovered (for a tooltip listing what it holds)
	};

	// A switch (if it has one), the name on an accent bar that folds, and the badge at its right end. Red when off.
	inline GroupState GroupBar(const char* a_id, const Group& a_group)
	{
		GroupState state;
		ImGui::PushID(a_id);
		ImVec2 avail{};
		ImGui::GetContentRegionAvail(&avail);
		const float left = ImGui::GetCursorPosX();
		const bool  off = a_group.on && !*a_group.on;
		if (a_group.on) {
			ImGui::BeginDisabled(a_group.locked);
			state.toggled = ImGui::Checkbox("##on", a_group.on);
			ImGui::EndDisabled();
			Tip(a_group.label, a_group.switchTip);
			ImGui::SameLine();
		}
		{
			HeaderLook look(off);
			if (off) {
				ImGui::PushStyleColor(ImGuiCol_Text, kOff);
			}
			if (a_group.fold != 0) {
				ImGui::SetNextItemOpen(a_group.fold > 0, ImGuiCond_Always);
			}
			state.open = ImGui::CollapsingHeader(std::format("{}###bar", a_group.label).c_str(),
				a_group.openFirst ? ImGuiTreeNodeFlags_DefaultOpen : 0);
			if (off) {
				ImGui::PopStyleColor();
			}
		}
		state.hovered = ImGui::IsItemHovered();
		if (a_group.scroll) {
			ImGui::SetScrollHereY(0.0f);
		}
		if (!a_group.badge.empty()) {
			ImVec2 size{};
			ImGui::CalcTextSize(&size, a_group.badge.c_str(), nullptr, false, -1.0f);
			ImGui::SameLine(left + avail.x - size.x - ImGui::GetStyle()->FramePadding.x * 2.0f);
			ImGui::TextColored(a_group.badgeColour, "%s", a_group.badge.c_str());
		}
		ImGui::PopID();
		return state;
	}

	// ---- settings rows: a table of four columns (switch, name, value, Reset) ----
	inline bool BeginRows(const char* a_id)
	{
		if (!ImGui::BeginTable(a_id, 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings)) {
			return false;
		}
		ImVec2 reset{};
		ImGui::CalcTextSize(&reset, "Reset", nullptr, true, -1.0f);
		ImGui::TableSetupColumn("##on", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, reset.x + ImGui::GetStyle()->FramePadding.x * 2.0f);
		return true;
	}

	inline void EndRows() { ImGui::EndTable(); }

	// The pieces of a row, for a mod that draws its own value cell. In order: RowBegin, (its switch), RowName,
	// (its value, after ImGui::TableNextColumn), RowReset, RowEnd.
	inline void RowBegin(int a_id)
	{
		ImGui::PushID(a_id);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
	}

	// The name cell: red and struck through when off, and under it in blue what differs (a_changed), if anything.
	// Returns whether the name is hovered.
	inline bool RowName(const char* a_label, bool a_off = false, const char* a_changed = nullptr)
	{
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		if (a_off) {
			ImGui::PushStyleColor(ImGuiCol_Text, kOff);
		}
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(a_label);
		ImGui::PopTextWrapPos();
		if (a_off) {
			ImGui::PopStyleColor();
			Strike();
		}
		const bool hovered = ImGui::IsItemHovered();
		if (a_changed && *a_changed) {
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(kChanged, "%s", a_changed);
			ImGui::PopTextWrapPos();
		}
		return hovered;
	}

	// The last cell: Reset when a_show. True when pressed.
	inline bool RowReset(bool a_show, const char* a_tip = nullptr)
	{
		ImGui::TableNextColumn();
		if (!a_show) {
			return false;
		}
		const bool pressed = ImGui::SmallButton("Reset");
		Tip("Reset", a_tip);
		return pressed;
	}

	inline void RowEnd() { ImGui::PopID(); }

	// A whole yes/no row. a_default (if given) is what Reset puts back; a_changed is the blue note under the name
	// while it differs. True when the value changed, by the tick box or by Reset.
	inline bool ToggleRow(int a_id, const char* a_label, const char* a_help, bool* a_value, const bool* a_default = nullptr,
		const char* a_changed = nullptr, bool a_locked = false)
	{
		RowBegin(a_id);
		ImGui::BeginDisabled(a_locked);
		bool changed = ImGui::Checkbox("##on", a_value);
		ImGui::EndDisabled();
		bool       hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
		const bool differs = a_default && *a_default != *a_value;
		hovered = RowName(a_label, !*a_value, differs ? a_changed : nullptr) || hovered;
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(*a_value ? kGood : kOff, "%s", *a_value ? "On" : "Off");
		hovered = hovered || ImGui::IsItemHovered();
		if (RowReset(differs && !a_locked)) {
			*a_value = *a_default;
			changed = true;
		}
		if (hovered && a_help && *a_help && TipBegin(a_label)) {
			ImGui::TextUnformatted(a_help);
			TipEnd();
		}
		RowEnd();
		return changed;
	}

	// A whole slider row, as ToggleRow.
	inline bool SliderRow(int a_id, const char* a_label, const char* a_help, float* a_value, float a_min, float a_max, const char* a_format,
		const float* a_default = nullptr, const char* a_changed = nullptr, bool a_locked = false)
	{
		RowBegin(a_id);
		const bool differs = a_default && *a_default != *a_value;
		bool       hovered = RowName(a_label, false, differs ? a_changed : nullptr);
		ImGui::TableNextColumn();
		ImGui::BeginDisabled(a_locked);
		ImGui::SetNextItemWidth(-FLT_MIN);
		bool changed = ImGui::SliderFloat("##value", a_value, a_min, a_max, a_format);
		ImGui::EndDisabled();
		hovered = hovered || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
		if (RowReset(differs && !a_locked)) {
			*a_value = *a_default;
			changed = true;
		}
		if (hovered && a_help && *a_help && TipBegin(a_label)) {
			ImGui::TextUnformatted(a_help);
			ImGui::TextColored(kDim, "%s", "Ctrl+click the slider to type a value.");
			TipEnd();
		}
		RowEnd();
		return changed;
	}

	// ---- the mod's own on/off, at the top of its first page ----
	// A tick box with the name beside it in large-ish letters and ON (green) or OFF (red) after it, then one line on
	// what on or off means. True when it was pressed: *a_on holds the new value.
	inline bool MasterSwitch(const char* a_label, bool* a_on, const char* a_whenOn, const char* a_whenOff, const char* a_help = nullptr)
	{
		ImGui::PushID("master");
		const bool changed = ImGui::Checkbox("##on", a_on);
		bool       hovered = ImGui::IsItemHovered();
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(*a_on ? Accent() : kOff, "%s", a_label);
		hovered = hovered || ImGui::IsItemHovered();
		ImGui::SameLine();
		Chip(*a_on ? "ON" : "OFF", *a_on ? kGood : kOff);
		if (const char* line = *a_on ? a_whenOn : a_whenOff; line && *line) {
			Wrapped(*a_on ? kDim : kOff, line);
		}
		if (hovered && a_help && *a_help && TipBegin(a_label)) {
			ImGui::TextUnformatted(a_help);
			TipEnd();
		}
		ImGui::Spacing();
		ImGui::PopID();
		return changed;
	}

	// ---- the last line of a page: the mod, its version, and whether it is on ----
	inline void Footer(bool a_on, const char* a_extra = nullptr)
	{
		const auto& l = detail::look;
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(kDim, "%s %s", l.mod, l.version);
		ImGui::SameLine();
		ImGui::TextColored(a_on ? kGood : kOff, "%s", a_on ? "on" : "off");
		if (a_extra && *a_extra) {
			ImGui::SameLine();
			ImGui::TextColored(kDim, "|  %s", a_extra);
		}
	}
}
