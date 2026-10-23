#include "SettingsUI.h"

#include "Extras.h"
#include "Hotkey.h"
#include "Needs.h"
#include "Settings.h"

// SKSEMenuFramework.h is the SKSE Menu Framework author's consumer header (MIT),
// in lib/SKSEMenuFramework/ with its licence. Only this file includes it.
#pragma warning(push, 0)
#include "SKSEMenuFramework.h"
#pragma warning(pop)

// Two pages (Tom, TEST10: "just General and Icons and then collapsible menus for
// each icon in the icons tab? Then collapsible menus for relevant stuff in General
// too"). Longer explanations sit behind a (?) that shows them on hover.
namespace SettingsUI
{
	namespace
	{
		void Hint(const char* a_text)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", a_text);
			}
		}

		void Footer(void (*a_restore)(), const char* a_restoreText)
		{
			ImGui::Separator();
			if (ImGui::Button("Save settings")) {
				Settings::Save();
			}
			if (Settings::HasUnsavedChanges()) {
				ImGui::SameLine();
				ImGui::TextDisabled("Not saved yet");
			}
			ImGui::SameLine();
			if (ImGui::Button(a_restoreText)) {
				a_restore();
			}
			Hint("Save settings saves both pages at once.");
		}

		// The Custom colours, one swatch per stage. Click a swatch for the picker, or
		// type the hex. ColorEdit4 with NoAlpha is what ImGui's ColorEdit3 does
		// itself; the header's ColorEdit3 wrapper passes col[3] instead of col.
		bool CustomColors(std::array<std::uint32_t, 6>& a_colors)
		{
			static constexpr std::array<const char*, 6> LABELS{
				"1. Fine##c0", "2.##c1", "3.##c2", "4.##c3", "5.##c4", "6. Worst##c5"
			};
			bool changed = false;
			for (std::size_t k = 0; k < a_colors.size(); ++k) {
				const auto c = a_colors[k];
				float rgba[4]{ ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f };
				if (ImGui::ColorEdit4(LABELS[k], rgba, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayHex)) {
					const auto byte = [](float a_v) { return static_cast<std::uint32_t>(std::lround(std::clamp(a_v, 0.0f, 1.0f) * 255.0f)); };
					a_colors[k] = (byte(rgba[0]) << 16) | (byte(rgba[1]) << 8) | byte(rgba[2]);
					changed = true;
				}
			}
			ImGui::Text("Start from");
			ImGui::SameLine();
			if (ImGui::Button("Bone to red##copy")) {
				a_colors = Settings::StageColors(Settings::ColorScheme::kBoneToRed);
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Green to red##copy")) {
				a_colors = Settings::StageColors(Settings::ColorScheme::kGreenToRed);
				changed = true;
			}
			Hint("Stage 1 is fine and stage 6 is the worst. For hunger: Well Fed, Satisfied, Peckish, Hungry, Famished, Ravenous.");
			return changed;
		}

		// Presets (Tom, 2026-10-21): a whole look and layout in one click. Players on
		// other UIs save theirs to a file and send it in to be added.
		std::string exported;  // what the last export said, shown under its button

		void Presets()
		{
			if (ImGui::Button("Default##preset")) {
				Settings::RestoreAllDefaults();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%s", Settings::MatchesDefaults() ? "In use. The mod's own layout." : "The mod's own layout.");
			const auto presets = Settings::GetPresets();
			for (std::size_t k = 0; k < presets.size(); ++k) {
				const auto& p = presets[k];
				if (ImGui::Button(std::format("{}##preset{}", p.name, k).c_str())) {
					Settings::ApplyPreset(p);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("%s%s", Settings::IsInUse(p) ? "In use. " : "", p.note.c_str());
			}
			ImGui::Spacing();
			if (ImGui::Button("Save my settings as a preset")) {
				const auto path = Settings::ExportPreset();
				exported = path.empty() ? std::string("Could not write the file. NeedsWidget.log says why.") :
				                          std::format("Saved as {}. Send that file to have it added for everyone.", path.filename().string());
			}
			ImGui::SameLine();
			if (ImGui::Button("Look for new presets")) {
				Settings::ScanPresets();
			}
			Hint("A preset changes every setting at once, and nothing is kept until you press Save settings.\nPresets live in Data/SKSE/Plugins/NeedsWidget/Presets/.");
			if (!exported.empty()) {
				ImGui::TextDisabled("%s", exported.c_str());
			}
		}

		bool Look(Settings::General& g)
		{
			bool changed = false;
			auto look = static_cast<int>(g.look);
			auto colors = static_cast<int>(g.colors);
			changed |= ImGui::RadioButton("Ring", &look, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Bar", &look, 1);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Icon only", &look, 2);
			changed |= ImGui::Checkbox("Colour the icons as needs get worse", &g.tintArt);
			ImGui::SeparatorText("Colours");
			changed |= ImGui::RadioButton("Bone to red", &colors, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Green to red", &colors, 1);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Custom", &colors, 2);
			if (colors == 2) {
				changed |= CustomColors(g.customColors);
			}
			changed |= ImGui::Checkbox("Pulse an icon while that need is at its worst", &g.pulseWorst);
			changed |= ImGui::Checkbox("Flash an icon when its need gets worse", &g.flashWorse);
			g.look = static_cast<Settings::Look>(look);
			g.colors = static_cast<Settings::ColorScheme>(colors);
			return changed;
		}

		bool Layout(Settings::General& g)
		{
			bool changed = false;
			auto arrangement = static_cast<int>(g.arrangement);
			auto farEnd = g.fromFarEnd ? 1 : 0;
			changed |= ImGui::RadioButton("Row", &arrangement, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Column", &arrangement, 1);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Free", &arrangement, 2);
			Hint("Free: place each icon yourself, on the Icons page.");
			changed |= ImGui::SliderFloat("Size of all icons", &g.groupSize, 25.0f, 300.0f, "%.0f%%");
			if (arrangement != 2) {
				const bool row = arrangement == 0;
				changed |= ImGui::RadioButton(row ? "Start from the left" : "Start from the top", &farEnd, 0);
				ImGui::SameLine();
				changed |= ImGui::RadioButton(row ? "Start from the right" : "Start from the bottom", &farEnd, 1);
				changed |= ImGui::SliderFloat("Position by X", &g.lineX, 0.0f, 100.0f, "%.1f%%");
				changed |= ImGui::SliderFloat("Position by Y", &g.lineY, 0.0f, 100.0f, "%.1f%%");
				Hint("The position is the icon at the end you start from.");
				changed |= ImGui::SliderFloat("Spacing", &g.spacing, 0.0f, 200.0f, "%.0f%%");
				Hint("A share of an icon's size.");
				// A float slider, not SliderInt: the dll only calls what SMF exports.
				float perLine = static_cast<float>(g.perLine);
				changed |= ImGui::SliderFloat(row ? "Icons per row" : "Icons per column", &perLine, 0.0f,
					static_cast<float>(Needs::kCount), g.perLine == 0 ? "No limit" : "%.0f");
				g.perLine = static_cast<int>(perLine + 0.5f);
				Hint(row ? "Any more start a new row." : "Any more start a new column.");
				if (g.perLine > 0) {
					auto before = g.wrapBefore ? 1 : 0;
					changed |= ImGui::RadioButton(row ? "New rows go above" : "New columns go left", &before, 1);
					ImGui::SameLine();
					changed |= ImGui::RadioButton(row ? "New rows go below" : "New columns go right", &before, 0);
					g.wrapBefore = before == 1;
					changed |= ImGui::SliderFloat(row ? "Gap between rows" : "Gap between columns", &g.lineGap, 0.0f, 200.0f, "%.0f%%");
					Hint("A share of the biggest icon's size.");
				}
				changed |= ImGui::Checkbox("Close the gaps when an icon is hidden", &g.closeGaps);

				ImGui::SeparatorText(row ? "Order, left to right" : "Order, top to bottom");
				// Only the needs whose mod is in the game: Up and Down move past the
				// others, which keep their place in the saved order.
				std::vector<std::size_t> listed;
				for (std::size_t k = 0; k < g.order.size(); ++k) {
					if (Needs::Installed(static_cast<Needs::Id>(g.order[k]))) {
						listed.push_back(k);
					}
				}
				for (std::size_t n = 0; n < listed.size(); ++n) {
					const auto k = listed[n];
					// "##" keeps each button's id apart without showing the number.
					if (ImGui::Button(std::format("Up##up{}", n).c_str()) && n > 0) {
						std::swap(g.order[k], g.order[listed[n - 1]]);
						changed = true;
					}
					ImGui::SameLine();
					if (ImGui::Button(std::format("Down##down{}", n).c_str()) && n + 1 < listed.size()) {
						std::swap(g.order[k], g.order[listed[n + 1]]);
						changed = true;
					}
					ImGui::SameLine();
					ImGui::Text("%d. %s", static_cast<int>(n + 1), Needs::Name(static_cast<Needs::Id>(g.order[k])));
				}
			} else {
				changed |= ImGui::SliderFloat("Move all by X", &g.groupX, -100.0f, 100.0f, "%.1f%%");
				changed |= ImGui::SliderFloat("Move all by Y", &g.groupY, -100.0f, 100.0f, "%.1f%%");
				Hint("Size scales the icons and the gaps between them. Each icon's own section still moves it within the group.");
			}
			g.arrangement = static_cast<Settings::Arrangement>(arrangement);
			g.fromFarEnd = farEnd == 1;
			return changed;
		}

		bool Numbers(Settings::General& g)
		{
			bool changed = false;
			if (ImGui::Button("Show every number")) {
				Settings::ShowAllNumbers(true);
			}
			ImGui::SameLine();
			if (ImGui::Button("Hide every number")) {
				Settings::ShowAllNumbers(false);
			}
			Hint("Each icon's section on the Icons page can still turn its own number on or off.");
			auto above = g.numbersAbove ? 1 : 0;
			changed |= ImGui::RadioButton("Under the icons", &above, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Above the icons", &above, 1);
			changed |= ImGui::RadioButton("Show the percent", &g.numberShows, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Show the stage's name", &g.numberShows, 1);
			changed |= ImGui::SliderFloat("Number size", &g.numberSize, 50.0f, 400.0f, "%.0f%%");
			Hint("On top of the icons' own size.");
			changed |= ImGui::SliderFloat("Distance from the icon", &g.numberGap, -30.0f, 60.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move sideways", &g.numberX, -80.0f, 80.0f, "%.0f");
			changed |= ImGui::Checkbox("Colour the numbers like their icon", &g.numberTint);
			changed |= ImGui::Checkbox("Dark edge round the numbers", &g.numberEdge);
			Hint("Easier to read over snow and sky.");
			g.numbersAbove = above == 1;
			return changed;
		}

		bool Fading(Settings::General& g)
		{
			bool changed = false;
			changed |= ImGui::Checkbox("Fade out when nothing has changed for a while", &g.fadeWhenStill);
			Hint("Any change brings the icons back: a need getting better or worse, or an icon coming or going.");
			if (g.fadeWhenStill) {
				changed |= ImGui::SliderFloat("Fade out after", &g.fadeAfter, 2.0f, 120.0f, "%.0f seconds");
				changed |= ImGui::Checkbox("Stay on screen while a need is at its worst two stages", &g.keepBad);
			}
			return changed;
		}

		bool HotkeySection(Settings::General& g)
		{
			bool changed = false;
			if (Hotkey::Capturing()) {
				for (int k = ImGuiKey_NamedKey_BEGIN; k <= ImGuiKey_KeypadEqual; ++k) {
					if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) {
						Hotkey::Offer(k);
					}
				}
			}
			if (const int key = Hotkey::TakeCaptured(); key > 0) {
				g.hotkey = key;
				changed = true;
			}
			const auto keyText = Hotkey::Capturing() ?
			                         std::string(Hotkey::Refused() ? "That's SKSE Menu Framework's key: press another (Esc: keep the old one)" :
			                                                         "Press a key (Esc: keep the old one)") :
			                         std::format("Key: {}", g.hotkey > 0 ? Hotkey::KeyName(g.hotkey) : std::string("none"));
			if (ImGui::Button(keyText.c_str())) {
				Hotkey::StartCapture();
			}
			if (g.hotkey > 0) {
				ImGui::SameLine();
				if (ImGui::Button("No key")) {
					g.hotkey = 0;
					changed = true;
				}
			}
			changed |= ImGui::RadioButton("Hides and shows the widget", &g.hotkeyMode, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Brings it back for a while", &g.hotkeyMode, 1);
			Hint("Bringing it back only matters with fading on: the widget is always up otherwise.");
			return changed;
		}

		void __stdcall RenderGeneral()
		{
			Settings::MarkEditing();
			auto g = Settings::GetGeneral();
			bool changed = false;
			if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
				Presets();
			}
			if (ImGui::CollapsingHeader("Look")) {
				changed |= Look(g);
			}
			if (ImGui::CollapsingHeader("Layout")) {
				changed |= Layout(g);
			}
			if (ImGui::CollapsingHeader("Numbers")) {
				changed |= Numbers(g);
			}
			if (ImGui::CollapsingHeader("Fading")) {
				changed |= Fading(g);
			}
			if (ImGui::CollapsingHeader("Hotkey")) {
				changed |= HotkeySection(g);
			}
			if (changed) {
				Settings::SetGeneral(g);
			}
			Footer(Settings::RestoreGeneralDefaults, "Restore General's defaults");
		}

		void Icon(Needs::Id a_id)
		{
			auto icon = Settings::Get(a_id);
			bool changed = false;
			const auto r = Needs::Last(a_id);
			if (r.available) {
				ImGui::TextDisabled("Now: %s, %.0f%%, from %s", r.stageName ? r.stageName : "", r.fraction * 100.0f, r.source);
			} else {
				ImGui::TextDisabled("%s", Needs::WhyHidden(a_id).c_str());
				Hint("While this menu is open it shows faded, so you can still place it.");
			}
			changed |= ImGui::Checkbox("Show this icon", &icon.enabled);
			changed |= ImGui::Checkbox("Show the number", &icon.showNumber);
			changed |= ImGui::Checkbox("Hide when fine", &icon.hideWhenFine);
			ImGui::SameLine();
			ImGui::TextDisabled("(fine = %s)", Needs::FineMeaning(a_id).c_str());
			changed |= ImGui::SliderFloat("Size", &icon.size, 25.0f, 300.0f, "%.0f%%");
			changed |= ImGui::SliderFloat("Opacity", &icon.opacity, 0.0f, 100.0f, "%.0f%%");
			if (Settings::GetGeneral().arrangement == Settings::Arrangement::kFree) {
				changed |= ImGui::SliderFloat("Position by X", &icon.x, 0.0f, 100.0f, "%.1f%%");
				changed |= ImGui::SliderFloat("Position by Y", &icon.y, 0.0f, 100.0f, "%.1f%%");
			}
			if (changed) {
				Settings::Set(a_id, icon);
			}
			if (ImGui::Button("Restore this icon's defaults")) {
				Settings::RestoreDefaults(a_id);
			}
		}

		// The page an icon's section is on: "" for the Icons page, else the page an
		// extra icon's file asked for (Tom, 2026-10-21: the NSFW icons apart from the rest).
		std::string_view PageOf(Needs::Id a_id)
		{
			const auto* extra = Extras::Get(a_id);
			return extra ? std::string_view(extra->page) : std::string_view();
		}

		void RestoreIconsOn(std::string_view a_page)
		{
			for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
				if (Needs::Used(static_cast<Needs::Id>(id)) && PageOf(static_cast<Needs::Id>(id)) == a_page) {
					Settings::RestoreDefaults(static_cast<Needs::Id>(id));
				}
			}
		}

		void RestoreEveryIcon()
		{
			RestoreIconsOn({});
		}

		// One section per need on this page whose mod is in the game, in the row's
		// order. The header carries the current stage; "###" keeps its id the same as
		// that changes.
		void IconSections(std::string_view a_page)
		{
			Settings::MarkEditing();
			if (Settings::GetGeneral().arrangement != Settings::Arrangement::kFree) {
				ImGui::TextDisabled("The row or column on the General page places the icons.");
				Hint("Pick Free under General > Layout to place each icon yourself.");
			}
			bool any = false;
			for (const auto id : Settings::GetGeneral().order) {
				const auto need = static_cast<Needs::Id>(id);
				if (!Needs::Installed(need) || PageOf(need) != a_page) {
					continue;
				}
				any = true;
				const auto r = Needs::Last(need);
				const auto title = r.available && r.stageName ? std::format("{} ({})###need{}", Needs::Name(need), r.stageName, id) :
				                                                std::format("{} (hidden)###need{}", Needs::Name(need), id);
				ImGui::PushID(Needs::Name(need));
				if (ImGui::CollapsingHeader(title.c_str())) {
					Icon(need);
				}
				ImGui::PopID();
			}
			if (!any) {
				// An addon's page with none of its mods in the game.
				ImGui::TextDisabled("None of the mods these icons read are in this game.");
				std::string names;
				for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
					const auto need = static_cast<Needs::Id>(id);
					if (Needs::Used(need) && PageOf(need) == a_page) {
						names += std::format("{}: {}\n", Needs::Name(need), Needs::WhyHidden(need));
					}
				}
				if (!names.empty()) {
					Hint(names.c_str());
				}
			}
		}

		void __stdcall RenderIcons()
		{
			IconSections({});
			Footer(RestoreEveryIcon, "Restore every icon's defaults");
		}

		// Pages that extra icons asked for, by their place in Extras::Pages(). SKSE Menu
		// Framework takes a plain function per page, hence one per place.
		template <std::size_t N>
		void __stdcall RenderExtraPage()
		{
			const auto pages = Extras::Pages();
			if (N >= pages.size()) {
				return;
			}
			IconSections(pages[N]);
			Footer([] { RestoreIconsOn(Extras::Pages()[N]); }, "Restore these icons' defaults");
		}

		constexpr std::array EXTRA_PAGES{ &RenderExtraPage<0>, &RenderExtraPage<1>, &RenderExtraPage<2>, &RenderExtraPage<3> };
	}

	void Register()
	{
		// The header grabs SKSE Menu Framework's module handle when this dll loads.
		// Plugins load alphabetically, so NeedsWidget.dll comes first and that handle is
		// still null. Everything is loaded by now (kPostLoad), so take it again.
		menuFramework = GetModuleHandleW(L"SKSEMenuFramework");
		if (!menuFramework) {
			logs::warn("SKSE Menu Framework not loaded - no settings page");
			return;
		}

		SKSEMenuFramework::SetSection("Needs Widget AIO");
		SKSEMenuFramework::AddSectionItem("General", RenderGeneral);
		SKSEMenuFramework::AddSectionItem("Icons", RenderIcons);
		// Extra icons' own pages exist whenever an addon file asks for one: having
		// installed the addon is the player's choice.
		const auto pages = Extras::Pages();
		for (std::size_t k = 0; k < pages.size() && k < EXTRA_PAGES.size(); ++k) {
			SKSEMenuFramework::AddSectionItem(pages[k], EXTRA_PAGES[k]);
			logs::info("extra page {} registered", pages[k]);
		}
		logs::info("settings pages registered with SKSE Menu Framework");
	}

	void RegisterNeeds()
	{
		// The Icons page shows a section for each need whose mod is in the game; this
		// only logs which, now that the plugins are known.
		std::string added;
		std::string left;
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			const auto need = static_cast<Needs::Id>(id);
			if (!Needs::Used(need)) {
				continue;
			}
			auto& list = Needs::Installed(need) ? added : left;
			list += list.empty() ? Needs::Name(need) : std::string(", ") + Needs::Name(need);
		}
		logs::info("icon sections: {}; none (mod not in the game): {}", added.empty() ? "none" : added, left.empty() ? "none" : left);
	}
}
