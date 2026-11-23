#include "SettingsUI.h"

#include "Extras.h"
#include "Hotkey.h"
#include "HudLayer.h"
#include "Needs.h"
#include "Settings.h"
#include "Styles.h"

// SKSEMenuFramework.h is the SKSE Menu Framework author's consumer header (MIT),
// in lib/SKSEMenuFramework/ with its licence. Only this file includes it.
#pragma warning(push, 0)
#include "SKSEMenuFramework.h"
#pragma warning(pop)

// The look shared by Tom's mods. It draws through the header above, so it comes after it.
#include "FamilyUI.h"

// Two pages (Tom, TEST10: "just General and Icons and then collapsible menus for
// each icon in the icons tab? Then collapsible menus for relevant stuff in General
// too"). Since the family look: each group is a bar that folds, each setting a line with
// its own Reset, and one search box looks through every page. Explanations show on hover.
namespace SettingsUI
{
	namespace
	{
		using FamilyUI::kChanged;
		using FamilyUI::kDim;
		using FamilyUI::kGood;
		using FamilyUI::kOff;
		using FamilyUI::kWarning;

		void Hint(const char* a_text)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			FamilyUI::Tip(nullptr, a_text);
		}

		bool saveFailed = false;  // the last Save couldn't write the file

		// Whether everything is saved, in the family's colours: orange is a warning, green is saved.
		// Bright, not greyed out: players took moving the sliders for saving (2.3.0).
		void SaveState(bool a_unsaved)
		{
			if (a_unsaved) {
				ImGui::TextColored(kWarning, "%s", saveFailed ? "Could not save. NeedsWidget.log says why." : "Not saved yet. Press Save settings to keep it.");
			} else {
				ImGui::TextColored(kGood, "%s", "Saved.");
			}
		}

		// The bottom of a settings page: Save, the page's restore button, whether it's saved, then the mod's line.
		void SaveBar(void (*a_restore)(), const char* a_restoreText)
		{
			ImGui::Spacing();
			ImGui::Separator();
			const bool unsaved = Settings::HasUnsavedChanges();
			if (FamilyUI::AccentButton("Save settings", unsaved)) {
				saveFailed = !Settings::Save();
			}
			ImGui::SameLine();
			if (ImGui::Button(a_restoreText)) {
				a_restore();
			}
			Hint("Save settings saves every page at once.");
			ImGui::SameLine();
			SaveState(unsaved);
			FamilyUI::Footer(Settings::GetGeneral().showWidget);
		}

		// Drag icons (Tom, testing 2.4.1: "make it so people could just drag the icons
		// around the screen"). A see-through window over the whole screen with a handle
		// on each icon showing. SKSE Menu Framework's own window closes while it's up,
		// so it hides no icons, and opens again on Done. TEST25 (Tom: "options to put it
		// vertical, move them individually etc"): the layout, the order, snapping, the
		// arrow keys, a right-click menu per icon and Cancel, all in the one screen.
		SKSEMenuFramework::Model::WindowInterface* dragWindow = nullptr;

		// GetMainWindow isn't in the header; every SMF 3 exports it (check_smf_exports
		// can't see it, so a missing one only means the menu stays open).
		SKSEMenuFramework::Model::WindowInterface* MainWindow()
		{
			using func_t = SKSEMenuFramework::Model::WindowInterface* (*)();
			const auto func = reinterpret_cast<func_t>(GetProcAddress(menuFramework, "GetMainWindow"));
			return func ? func() : nullptr;
		}

		// How it all was when the drag screen opened, for Cancel.
		Settings::General                         atOpen{};
		std::array<Settings::Icon, Needs::kCount> atOpenIcons{};

		bool          freeAll = false;  // Free: dragging moves (and the corner sizes) all of them, not just that icon
		bool          reorder = false;  // a row or column: dragging changes the icon's place in the order
		bool          snap = true;      // line up with the other icons and the middle of the screen
		std::uint32_t nudgeId = UINT32_MAX;  // the icon last dragged, for the arrow keys
		std::uint32_t menuId = UINT32_MAX;   // the icon whose right-click menu is open

		void StartDragging()
		{
			if (!dragWindow) {
				return;
			}
			atOpen = Settings::GetGeneral();
			for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
				atOpenIcons[id] = Settings::Get(static_cast<Needs::Id>(id));
			}
			nudgeId = UINT32_MAX;
			HudLayer::WatchSpots(true);
			dragWindow->IsOpen = true;
			if (auto* main = MainWindow()) {
				main->IsOpen = false;
			}
			logs::info("drag screen opened");
		}

		void StopDragging(bool a_keep)
		{
			if (!a_keep) {
				Settings::SetGeneral(atOpen);
				for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
					Settings::Set(static_cast<Needs::Id>(id), atOpenIcons[id]);
				}
			}
			// The menu first, so the game never unpauses for a frame in between.
			if (auto* main = MainWindow()) {
				main->IsOpen = true;
			}
			dragWindow->IsOpen = false;
			HudLayer::WatchSpots(false);
			logs::info("drag screen closed{}", a_keep ? "" : ", changes undone");
		}

		void DragButton()
		{
			if (!dragWindow) {
				return;
			}
			const bool on = Settings::GetGeneral().showWidget;  // nothing to drag while the widget is off
			ImGui::BeginDisabled(!on);
			if (ImGui::Button("Drag icons on screen")) {
				StartDragging();
			}
			ImGui::EndDisabled();
			Hint(on ? "Move the icons with the mouse, and drag their corners to resize them. Done brings this menu back." :
					  "The widget is switched off. Show the widget, on the General page, first.");
		}

		bool OneIcon()
		{
			return Settings::GetGeneral().arrangement == Settings::Arrangement::kFree && !freeAll;
		}

		// To Free: every icon starts where it is now, so nothing jumps. Size of all icons
		// and Move all are folded into each icon (back to 100% and 0), because a Free
		// position only reaches 0-100% of the screen before they scale it.
		void SwitchTo(Settings::Arrangement a_to, const std::vector<HudLayer::Spot>& a_spots)
		{
			auto g = Settings::GetGeneral();
			if (a_to == g.arrangement) {
				return;
			}
			if (a_to == Settings::Arrangement::kFree) {
				const float k = g.groupSize / 100.0f;
				for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
					const auto need = static_cast<Needs::Id>(id);
					auto       icon = Settings::Get(need);
					auto       placed = Settings::Place(icon, g);
					for (const auto& s : a_spots) {
						if (s.id == id) {
							placed.x = s.x * 100.0f;
							placed.y = s.y * 100.0f;
						}
					}
					icon.x = std::clamp(placed.x, 0.0f, 100.0f);
					icon.y = std::clamp(placed.y, 0.0f, 100.0f);
					icon.size = icon.size * k;
					Settings::Set(need, icon);
				}
				g.groupSize = 100.0f;
				g.groupX = 0.0f;
				g.groupY = 0.0f;
			}
			g.arrangement = a_to;
			Settings::SetGeneral(g);
		}

		// Moves by a_dx, a_dy (percent of the screen) from a starting point: the icon in
		// Free (Size of all icons scales its distance from the others, so the move is
		// divided by it), all of them, or the whole row or column.
		void MoveFrom(const Settings::General& a_general, const Settings::Icon& a_icon, Needs::Id a_id, float a_dx, float a_dy)
		{
			auto g = Settings::GetGeneral();
			if (g.arrangement == Settings::Arrangement::kFree && !freeAll) {
				auto        icon = Settings::Get(a_id);
				const float k = (std::max)(g.groupSize / 100.0f, 0.01f);
				icon.x = std::clamp(a_icon.x + a_dx / k, 0.0f, 100.0f);
				icon.y = std::clamp(a_icon.y + a_dy / k, 0.0f, 100.0f);
				Settings::Set(a_id, icon);
			} else if (g.arrangement == Settings::Arrangement::kFree) {
				g.groupX = std::clamp(a_general.groupX + a_dx, -100.0f, 100.0f);
				g.groupY = std::clamp(a_general.groupY + a_dy, -100.0f, 100.0f);
				Settings::SetGeneral(g);
			} else {
				g.lineX = std::clamp(a_general.lineX + a_dx, 0.0f, 100.0f);
				g.lineY = std::clamp(a_general.lineY + a_dy, 0.0f, 100.0f);
				Settings::SetGeneral(g);
			}
		}

		// The size the corner square changes: that icon's own, or all icons' (in a row
		// or column that keeps the spacing right). The sliders' range, 25 to 300%.
		float SizeOf(Needs::Id a_id)
		{
			return OneIcon() ? Settings::Get(a_id).size : Settings::GetGeneral().groupSize;
		}

		void SetSize(Needs::Id a_id, float a_size)
		{
			if (OneIcon()) {
				auto icon = Settings::Get(a_id);
				icon.size = std::clamp(a_size, 25.0f, 300.0f);
				Settings::Set(a_id, icon);
			} else {
				auto g = Settings::GetGeneral();
				g.groupSize = std::clamp(a_size, 25.0f, 300.0f);
				Settings::SetGeneral(g);
			}
		}

		// What the mouse is holding, and how things were when it took hold, so the icon
		// follows the mouse exactly however long the drag.
		enum class Grab
		{
			kNone,
			kMove,
			kResize,
			kOrder
		};
		struct Held
		{
			Grab              kind{ Grab::kNone };
			std::uint32_t     id{ UINT32_MAX };
			Settings::General general{};
			Settings::Icon    icon{};
			HudLayer::Spot    spot{};
			float             startSize{ 100.0f };
		};
		Held  held;
		float swapX = -1.0f;  // where the held icon was at its last swap in Reorder
		float swapY = -1.0f;

		void Take(Grab a_kind, const HudLayer::Spot& a_spot)
		{
			const auto id = static_cast<Needs::Id>(a_spot.id);
			held = { a_kind, a_spot.id, Settings::GetGeneral(), Settings::Get(id), a_spot, SizeOf(id) };
			swapX = swapY = -1.0f;
			nudgeId = a_spot.id;
		}

		// The held icon's place in the order follows the mouse: once the mouse is nearer
		// another icon of the line than this one, they swap. The widget shows a swap a
		// frame or two later, so nothing more swaps until this icon has moved (swapX,
		// swapY), or the stale places would swap it straight back.
		void Reorder(const std::vector<HudLayer::Spot>& a_spots, float a_w, float a_h)
		{
			ImVec2 m{};
			ImGui::GetMousePos(&m);
			const auto distance = [&](const HudLayer::Spot& a_s) {
				const float dx = a_s.x * a_w - m.x;
				const float dy = a_s.y * a_h - m.y;
				return dx * dx + dy * dy;
			};
			const HudLayer::Spot* own = nullptr;
			const HudLayer::Spot* nearest = nullptr;
			for (const auto& s : a_spots) {
				if (s.id == held.id) {
					own = &s;
				} else if (!nearest || distance(s) < distance(*nearest)) {
					nearest = &s;
				}
			}
			if (!own || (own->x == swapX && own->y == swapY)) {
				return;
			}
			swapX = swapY = -1.0f;
			if (!nearest || distance(*nearest) >= distance(*own)) {
				return;
			}
			auto       g = Settings::GetGeneral();
			const auto a = std::ranges::find(g.order, static_cast<int>(held.id));
			const auto b = std::ranges::find(g.order, static_cast<int>(nearest->id));
			if (a != g.order.end() && b != g.order.end()) {
				std::iter_swap(a, b);
				Settings::SetGeneral(g);
				swapX = own->x;
				swapY = own->y;
			}
		}

		// The box with the choices, Save, Done and Cancel. It moves by its title.
		void DragBox(const std::vector<HudLayer::Spot>& a_spots, float a_w, float a_h)
		{
			ImGui::SetNextWindowPos({ a_w * 0.5f, a_h * 0.05f }, ImGuiCond_FirstUseEver, { 0.5f, 0.0f });
			ImGui::Begin("Drag icons##NeedsWidgetDragBar", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);
			{
				FamilyUI::Page look;  // the mod's colour on the ticks and sliders; gone again before End
				auto g = Settings::GetGeneral();
				bool changed = false;

				auto arrangement = static_cast<int>(g.arrangement);
				const auto was = arrangement;
				ImGui::RadioButton("Row##drag", &arrangement, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Column##drag", &arrangement, 1);
				ImGui::SameLine();
				ImGui::RadioButton("Free##drag", &arrangement, 2);
				Hint("Free: each icon on its own. They start where they are now.");
				if (arrangement != was) {
					SwitchTo(static_cast<Settings::Arrangement>(arrangement), a_spots);
					g = Settings::GetGeneral();
				}

				const bool free = g.arrangement == Settings::Arrangement::kFree;
				const bool row = g.arrangement == Settings::Arrangement::kRow;
				auto       scope = free ? (freeAll ? 1 : 0) : (reorder ? 1 : 0);
				ImGui::Text("%s", "Dragging an icon moves");
				ImGui::SameLine();
				ImGui::RadioButton(free ? "just that icon##scope" : row ? "the whole row##scope" : "the whole column##scope", &scope, 0);
				ImGui::SameLine();
				ImGui::RadioButton(free ? "all of them##scope" : row ? "its place in the row##scope" : "its place in the column##scope", &scope, 1);
				(free ? freeAll : reorder) = scope == 1;

				if (!free) {
					auto farEnd = g.fromFarEnd ? 1 : 0;
					changed |= ImGui::RadioButton(row ? "Start from the left##drag" : "Start from the top##drag", &farEnd, 0);
					ImGui::SameLine();
					changed |= ImGui::RadioButton(row ? "Start from the right##drag" : "Start from the bottom##drag", &farEnd, 1);
					g.fromFarEnd = farEnd == 1;
					changed |= ImGui::SliderFloat("Spacing##drag", &g.spacing, 0.0f, 200.0f, "%.0f%%");
					float perLine = static_cast<float>(g.perLine);
					changed |= ImGui::SliderFloat(row ? "Icons per row##drag" : "Icons per column##drag", &perLine, 0.0f,
						static_cast<float>(Needs::kCount), g.perLine == 0 ? "No limit" : "%.0f");
					g.perLine = static_cast<int>(perLine + 0.5f);
				}
				if (changed) {
					Settings::SetGeneral(g);
				}
				ImGui::Checkbox("Snap to the other icons and the middle of the screen", &snap);

				ImGui::Separator();
				ImGui::TextDisabled("%s", OneIcon() ? "Corner square: make that icon bigger or smaller." : "Corner square: make them all bigger or smaller.");
				ImGui::TextDisabled("%s", "Right-click an icon for its own size, opacity and number.");
				ImGui::TextDisabled("%s", "Arrow keys: nudge the last one you moved.");
				if (a_spots.empty()) {
					ImGui::TextDisabled("%s", "No icons on screen to drag.");
				}

				ImGui::Separator();
				const bool unsaved = Settings::HasUnsavedChanges();
				if (FamilyUI::AccentButton("Save settings##drag", unsaved)) {
					saveFailed = !Settings::Save();
				}
				ImGui::SameLine();
				if (ImGui::Button("Done")) {
					StopDragging(true);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					StopDragging(false);
				}
				Hint("Cancel puts everything back as it was when this screen opened.\nDone keeps the changes. Save settings keeps them after you quit.");
				SaveState(unsaved);
			}
			ImGui::End();
		}

		// One icon's own settings, on a right-click. Whether it's open.
		bool IconMenu()
		{
			if (!ImGui::BeginPopup("##NeedsWidgetIconMenu")) {
				return false;
			}
			if (menuId < Needs::kCount) {
				const auto id = static_cast<Needs::Id>(menuId);
				auto       icon = Settings::Get(id);
				bool       changed = false;
				ImGui::SeparatorText(Needs::Name(id));
				changed |= ImGui::SliderFloat("Size##menu", &icon.size, 25.0f, 300.0f, "%.0f%%");
				changed |= ImGui::SliderFloat("Opacity##menu", &icon.opacity, 0.0f, 100.0f, "%.0f%%");
				changed |= ImGui::Checkbox("Show the number##menu", &icon.showNumber);
				if (icon.showNumber) {
					changed |= ImGui::SliderFloat("Number sideways##menu", &icon.numberX, -200.0f, 200.0f, "%.0f");
					changed |= ImGui::SliderFloat("Number up/down##menu", &icon.numberY, -120.0f, 120.0f, "%.0f");
				}
				changed |= ImGui::Checkbox("Hide when fine##menu", &icon.hideWhenFine);
				const bool hide = ImGui::Button("Hide this icon##menu");
				if (hide) {
					icon.enabled = false;
					changed = true;
					ImGui::CloseCurrentPopup();
				}
				Hint("Show this icon, in its section on the Icons page, brings it back.");
				if (Needs::HideFrom(id) >= 0) {
					changed |= ImGui::Checkbox((Needs::HideFromText(id) + "##menu").c_str(), &icon.hideFromStage);
				}
				if (changed) {
					Settings::Set(id, icon);
				}
				if (hide) {
					saveFailed = !Settings::SaveShown(id);
				}
			}
			ImGui::EndPopup();
			return true;
		}

		void __stdcall RenderDrag()
		{
			Settings::MarkEditing();  // hidden icons show faded, as on the pages
			const auto  screen = RE::BSGraphics::Renderer::GetScreenSize();  // what SMF sizes ImGui to
			const float w = static_cast<float>(screen.width);
			const float h = static_cast<float>(screen.height);
			const auto  spots = HudLayer::Spots();
			const bool  free = Settings::GetGeneral().arrangement == Settings::Arrangement::kFree;
			constexpr float SNAP_PX = 6.0f;

			ImGui::SetNextWindowPos({ 0.0f, 0.0f }, 0, { 0.0f, 0.0f });
			ImGui::SetNextWindowSize({ w, h });
			ImGui::Begin("##NeedsWidgetDrag", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
					ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);
			auto*       draw = ImGui::GetWindowDrawList();
			const ImU32 accent = ImGui::GetColorU32(FamilyUI::Accent());
			const ImU32 guide = ImGui::GetColorU32(FamilyUI::Fade(FamilyUI::Accent(), 0.56f));
			bool        anyHeld = false;
			for (const auto& s : spots) {
				const ImVec2 min{ (s.x - s.halfW) * w, (s.y - s.halfH) * h };
				const ImVec2 max{ (s.x + s.halfW) * w, (s.y + s.halfH) * h };
				if (max.x - min.x < 1.0f || max.y - min.y < 1.0f) {
					continue;
				}
				const auto id = static_cast<Needs::Id>(s.id);

				// The corner square first: where two overlap, the first one drawn gets the mouse.
				const float  grip = std::clamp((max.x - min.x) * 0.3f, 10.0f, 22.0f);
				const ImVec2 gripMin{ max.x - grip * 0.5f, max.y - grip * 0.5f };
				const ImVec2 gripMax{ max.x + grip * 0.5f, max.y + grip * 0.5f };
				ImGui::SetCursorScreenPos(gripMin);
				ImGui::InvisibleButton(std::format("##size{}", s.id).c_str(), { grip, grip });
				if (ImGui::IsItemActivated()) {
					Take(Grab::kResize, s);
				}
				const bool sizing = ImGui::IsItemActive() && held.kind == Grab::kResize && held.id == s.id;
				const bool gripHovered = ImGui::IsItemHovered();
				if (gripHovered && !sizing) {
					ImGui::SetTooltip("%s", OneIcon() ? "Drag to make this icon bigger or smaller" : "Drag to make every icon bigger or smaller");
				}
				if (sizing) {
					// The corner follows the mouse along the diagonal.
					ImVec2 d{};
					ImGui::GetMouseDragDelta(&d, 0, 0.0f);
					const float startHalf = held.spot.halfW * w;
					const float half = (std::max)(startHalf + (d.x + d.y) * 0.5f, 4.0f);
					SetSize(id, held.startSize * half / startHalf);
				}

				ImGui::SetCursorScreenPos(min);
				ImGui::InvisibleButton(std::format("##drag{}", s.id).c_str(), { max.x - min.x, max.y - min.y });
				if (ImGui::IsItemActivated()) {
					Take(!free && reorder ? Grab::kOrder : Grab::kMove, s);
				}
				const bool active = ImGui::IsItemActive() && held.id == s.id && (held.kind == Grab::kMove || held.kind == Grab::kOrder);
				const bool hovered = ImGui::IsItemHovered();
				if (ImGui::IsItemClicked(1)) {
					menuId = s.id;
					ImGui::OpenPopup("##NeedsWidgetIconMenu");
				}
				if (hovered && !active) {
					ImGui::SetTooltip("%s", Needs::Name(id));
				}
				if (active && held.kind == Grab::kOrder) {
					Reorder(spots, w, h);
				} else if (active) {
					// From where it was grabbed, plus the drag; snapped when the centre
					// comes within a few pixels of another icon's (moving one on its own)
					// or the middle of the screen.
					ImVec2 d{};
					ImGui::GetMouseDragDelta(&d, 0, 0.0f);
					float cx = held.spot.x * w + d.x;
					float cy = held.spot.y * h + d.y;
					float guideX = -1.0f;
					float guideY = -1.0f;
					if (snap) {
						const auto snapTo = [&](float& a_c, float a_target, float& a_guide) {
							if (std::abs(a_c - a_target) < SNAP_PX) {
								a_c = a_target;
								a_guide = a_target;
							}
						};
						if (OneIcon()) {
							for (const auto& other : spots) {
								if (other.id != s.id) {
									snapTo(cx, other.x * w, guideX);
									snapTo(cy, other.y * h, guideY);
								}
							}
						}
						snapTo(cx, w * 0.5f, guideX);
						snapTo(cy, h * 0.5f, guideY);
					}
					MoveFrom(held.general, held.icon, id, (cx - held.spot.x * w) / w * 100.0f, (cy - held.spot.y * h) / h * 100.0f);
					if (guideX >= 0.0f) {
						ImGui::ImDrawListManager::AddLine(draw, { guideX, 0.0f }, { guideX, h }, guide, 1.0f);
					}
					if (guideY >= 0.0f) {
						ImGui::ImDrawListManager::AddLine(draw, { 0.0f, guideY }, { w, guideY }, guide, 1.0f);
					}
				}
				anyHeld |= active || sizing;

				// ImGui colours are 0xAABBGGRR: the held one in the mod's own colour.
				const bool  busy = active || sizing;
				const ImU32 colour = busy ? accent : hovered || gripHovered ? 0xE0FFFFFF : 0x70FFFFFF;
				ImGui::ImDrawListManager::AddRect(draw, min, max, colour, 4.0f, 0, busy ? 3.0f : 1.5f);
				ImGui::ImDrawListManager::AddRectFilled(draw, gripMin, gripMax, sizing || gripHovered ? accent : colour, 2.0f, 0);
			}
			if (!anyHeld) {
				held.kind = Grab::kNone;
			}
			const bool menuOpen = IconMenu();
			ImGui::End();

			DragBox(spots, w, h);

			// The arrow keys nudge the last icon moved (or its row, or all of them) a
			// tenth of a percent, faster while held down.
			if (!anyHeld && !menuOpen && nudgeId < Needs::kCount) {
				const auto  id = static_cast<Needs::Id>(nudgeId);
				const float step = 0.1f;
				float       dx = 0.0f;
				float       dy = 0.0f;
				dx -= ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) ? step : 0.0f;
				dx += ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) ? step : 0.0f;
				dy -= ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) ? step : 0.0f;
				dy += ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) ? step : 0.0f;
				if (dx != 0.0f || dy != 0.0f) {
					MoveFrom(Settings::GetGeneral(), Settings::Get(id), id, dx, dy);
				}
			}
			if (!menuOpen && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {  // Esc on the menu only closes the menu
				StopDragging(true);
			}
		}

		// ---- the settings pages ----
		// One search box for every page: it looks through General's settings and every icon's.
		FamilyUI::Search search;
		std::string      version;  // "2.9.1", for the last line of a page

		constexpr const char* kSearchHint = "Search every setting: type a word, like opacity, fade or font";

		// What a search found. The lines are counted as they are drawn, so the line under the box
		// shows the frame before's count.
		struct Tally
		{
			std::string needle;
			std::size_t lines{ 0 };
			std::size_t groups{ 0 };
		};
		Tally tally;      // this frame's, being counted
		Tally lastTally;  // the last whole frame's

		void HelpTip(bool a_hovered, const char* a_label, const char* a_help)
		{
			if (a_hovered && a_help && *a_help && FamilyUI::TipBegin(a_label)) {
				ImGui::TextUnformatted(a_help);
				FamilyUI::TipEnd();
			}
		}

		// One group's lines. Drawing: each line is a row of the family's table (name, value, Reset),
		// with a blue note under the name while it differs from the default. While searching only
		// the lines that hold the word are drawn, under the group's name. Counting: nothing is
		// drawn or changed; it only adds up the group's lines and how many are changed, for the
		// group's bar. So everything a group draws has to sit behind one of these calls.
		class Rows
		{
		public:
			enum class Mode
			{
				kDraw,
				kCount
			};

			Rows(std::string a_heading, Mode a_mode) :
				heading(std::move(a_heading)), mode(a_mode)
			{}
			~Rows() { Leave(); }
			Rows(const Rows&) = delete;
			Rows& operator=(const Rows&) = delete;

			bool        Counting() const { return mode == Mode::kCount; }
			std::size_t Count() const { return count; }
			std::size_t Changed() const { return differing; }

			// A line with its own value cell: true when it is wanted, with the table ready for it.
			bool Line(std::string_view a_words, bool a_differs)
			{
				if (!Take(a_words, a_differs, true)) {
					return false;
				}
				if (!table) {
					table = FamilyUI::BeginRows(std::format("rows{}", tables).c_str());
				}
				return table;
			}

			// Something that isn't a row (buttons, a list) but a word can find. Ends the table.
			bool Block(std::string_view a_words, bool a_differs = false, bool a_counts = true)
			{
				if (!Take(a_words, a_differs, a_counts)) {
					return false;
				}
				Leave();
				return true;
			}

			// Notes and buttons that only show when not searching.
			bool Aside()
			{
				if (mode != Mode::kDraw || search.Active()) {
					return false;
				}
				Leave();
				return true;
			}

			// A tick box. a_redWhenOff: off is red and struck through, for a switch that hides something.
			bool Toggle(const char* a_label, const char* a_help, bool* a_value, bool a_default, bool a_redWhenOff = false)
			{
				const bool differs = *a_value != a_default;
				if (!Line(std::format("{} {}", a_label, a_help ? a_help : ""), differs)) {
					return false;
				}
				ImGui::PushID(a_label);
				FamilyUI::RowBegin(0);
				bool changed = ImGui::Checkbox("##on", a_value);
				bool hovered = ImGui::IsItemHovered();
				hovered = FamilyUI::RowName(a_label, a_redWhenOff && !*a_value, differs ? (a_default ? "Changed. Default: on" : "Changed. Default: off") : nullptr) || hovered;
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(*a_value ? kGood : a_redWhenOff ? kOff : kDim, "%s", *a_value ? "On" : "Off");
				hovered = hovered || ImGui::IsItemHovered();
				if (FamilyUI::RowReset(differs)) {
					*a_value = a_default;
					changed = true;
				}
				HelpTip(hovered, a_label, a_help);
				FamilyUI::RowEnd();
				ImGui::PopID();
				return changed;
			}

			// A slider. a_defaultFormat: how to write the default, when a_format only fits the value now.
			bool Slider(const char* a_label, const char* a_help, float* a_value, float a_default, float a_min, float a_max, const char* a_format,
				const char* a_defaultFormat = nullptr)
			{
				const bool differs = *a_value != a_default;
				if (!Line(std::format("{} {}", a_label, a_help ? a_help : ""), differs)) {
					return false;
				}
				char was[64]{};
				std::snprintf(was, sizeof was, a_defaultFormat ? a_defaultFormat : a_format, static_cast<double>(a_default));
				const auto note = std::format("Changed. Default: {}", was);
				ImGui::PushID(a_label);
				const bool changed = FamilyUI::SliderRow(0, a_label, a_help, a_value, a_min, a_max, a_format, &a_default, note.c_str());
				ImGui::PopID();
				return changed;
			}

			// One of a few choices, as round buttons side by side. *a_value is the choice's place in a_options.
			bool Pick(const char* a_label, const char* a_help, int* a_value, int a_default, std::span<const char* const> a_options)
			{
				const bool  differs = *a_value != a_default;
				std::string words = std::format("{} {}", a_label, a_help ? a_help : "");
				for (const char* option : a_options) {
					words += ' ';
					words += option;
				}
				if (!Line(words, differs)) {
					return false;
				}
				const char* was = a_default >= 0 && a_default < static_cast<int>(a_options.size()) ? a_options[static_cast<std::size_t>(a_default)] : "";
				const auto  note = std::format("Changed. Default: {}", was);
				ImGui::PushID(a_label);
				FamilyUI::RowBegin(0);
				const bool hovered = FamilyUI::RowName(a_label, false, differs ? note.c_str() : nullptr);
				ImGui::TableNextColumn();
				ImVec2 avail{};
				ImGui::GetContentRegionAvail(&avail);
				const float right = ImGui::GetCursorPosX() + avail.x;
				const auto* style = ImGui::GetStyle();
				bool        changed = false;
				int         k = 0;
				for (const char* option : a_options) {
					if (k > 0) {
						// Onto the next line when the cell is full.
						ImVec2 size{};
						ImGui::CalcTextSize(&size, option, nullptr, true, -1.0f);
						ImGui::SameLine();
						if (ImGui::GetCursorPosX() + ImGui::GetFrameHeight() + style->ItemInnerSpacing.x + size.x > right) {
							ImGui::NewLine();
						}
					}
					changed |= ImGui::RadioButton(option, a_value, k);
					++k;
				}
				if (FamilyUI::RowReset(differs)) {
					*a_value = a_default;
					changed = true;
				}
				HelpTip(hovered, a_label, a_help);
				FamilyUI::RowEnd();
				ImGui::PopID();
				return changed;
			}

			bool Pick(const char* a_label, const char* a_help, int* a_value, int a_default, std::initializer_list<const char*> a_options)
			{
				return Pick(a_label, a_help, a_value, a_default, std::span<const char* const>(a_options.begin(), a_options.size()));
			}

		private:
			bool Take(std::string_view a_words, bool a_differs, bool a_counts)
			{
				if (mode == Mode::kCount) {
					if (a_counts) {
						++count;
						differing += a_differs ? 1 : 0;
					}
					return false;
				}
				if (!search.Active()) {
					return true;
				}
				if (!search.Matches(std::format("{} {}", heading, a_words))) {
					return false;
				}
				if (!headed) {
					headed = true;
					++tally.groups;
					FamilyUI::Section(heading.c_str());
				}
				tally.lines += a_counts ? 1 : 0;
				return true;
			}

			void Leave()
			{
				if (table) {
					FamilyUI::EndRows();
					table = false;
					++tables;
				}
			}

			std::string heading;
			Mode        mode;
			bool        headed{ false };  // searching: the group's name is drawn
			bool        table{ false };
			int         tables{ 0 };      // tables ended so far, so each gets its own id
			std::size_t count{ 0 };
			std::size_t differing{ 0 };  // lines that differ from the default
		};

		// A group's bar when not searching: its name, and at the right end how many settings it
		// holds, or in blue how many differ from the defaults.
		void Badge(FamilyUI::Group& a_group, const Rows& a_counted)
		{
			if (a_counted.Changed() > 0) {
				a_group.badge = std::format("{} changed", a_counted.Changed());
				a_group.badgeColour = kChanged;
			} else {
				a_group.badge = FamilyUI::CountBadge(a_counted.Count());
			}
		}

		// The Custom colours, one line per stage. Click a swatch for the picker, or
		// type the hex. ColorEdit4 with NoAlpha is what ImGui's ColorEdit3 does
		// itself; the header's ColorEdit3 wrapper passes col[3] instead of col.
		bool CustomColors(Rows& a_rows, std::array<std::uint32_t, 6>& a_colors, const std::array<std::uint32_t, 6>& a_defaults)
		{
			static constexpr std::array<const char*, 6> LABELS{
				"Colour 1, fine", "Colour 2", "Colour 3", "Colour 4", "Colour 5", "Colour 6, worst"
			};
			static constexpr const char* HELP =
				"Stage 1 is fine and stage 6 is the worst. For hunger: Well Fed, Satisfied, Peckish, Hungry, Famished, Ravenous.\n"
				"Click the square for the picker, or type the hex.";
			bool changed = false;
			for (std::size_t k = 0; k < a_colors.size(); ++k) {
				const bool differs = a_colors[k] != a_defaults[k];
				if (!a_rows.Line(std::format("{} custom colours stage {}", LABELS[k], HELP), differs)) {
					continue;
				}
				const auto c = a_colors[k];
				const auto note = std::format("Changed. Default: {:06X}", a_defaults[k] & 0xFFFFFF);
				float      rgba[4]{ ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f };
				ImGui::PushID(LABELS[k]);
				FamilyUI::RowBegin(0);
				const bool hovered = FamilyUI::RowName(LABELS[k], false, differs ? note.c_str() : nullptr);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::ColorEdit4("##value", rgba, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayHex)) {
					const auto byte = [](float a_v) { return static_cast<std::uint32_t>(std::lround(std::clamp(a_v, 0.0f, 1.0f) * 255.0f)); };
					a_colors[k] = (byte(rgba[0]) << 16) | (byte(rgba[1]) << 8) | byte(rgba[2]);
					changed = true;
				}
				if (FamilyUI::RowReset(differs)) {
					a_colors[k] = a_defaults[k];
					changed = true;
				}
				HelpTip(hovered, LABELS[k], HELP);
				FamilyUI::RowEnd();
				ImGui::PopID();
			}
			if (a_rows.Block("custom colours start from bone to red green to red", false, false)) {
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Start the custom colours from");
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
			}
			return changed;
		}

		// Presets (Tom, 2026-10-21): a whole look and layout in one click. Players on
		// other UIs save theirs to a file and send it in to be added. The one in use is
		// the button in the mod's colour.
		std::string exported;  // what the last export said, shown under its button

		void Presets(Rows& a_rows, const std::vector<Settings::Preset>& a_presets)
		{
			std::string words = "preset presets default save my settings look for new";
			for (const auto& p : a_presets) {
				words += std::format(" {} {}", p.name, p.note);
			}
			if (!a_rows.Block(words, false, false)) {
				return;
			}
			const bool usual = Settings::MatchesDefaults();
			if (FamilyUI::Choice("Default##preset", usual)) {
				Settings::RestoreAllDefaults();
			}
			ImGui::SameLine();
			FamilyUI::Dim(usual ? "In use. The mod's own layout." : "The mod's own layout.");
			for (std::size_t k = 0; k < a_presets.size(); ++k) {
				const auto& p = a_presets[k];
				const bool  inUse = Settings::IsInUse(p);
				if (FamilyUI::Choice(std::format("{}##preset{}", p.name, k).c_str(), inUse)) {
					Settings::ApplyPreset(p);
				}
				ImGui::SameLine();
				FamilyUI::Dim(std::format("{}{}", inUse ? "In use. " : "", p.note).c_str());
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
			// 3.0 (Fable, Tom): share a preset as text in a comment or on Discord.
			if (ImGui::Button("Copy my settings")) {
				const auto text = Settings::PresetText();
				ImGui::SetClipboardText(text.c_str());
				exported = text.empty() ? std::string("Could not copy. NeedsWidget.log says why.") : std::string("Copied. Paste it anywhere to share it.");
			}
			ImGui::SameLine();
			if (ImGui::Button("Paste a preset")) {
				const char* text = ImGui::GetClipboardText();
				const auto  path = Settings::PastePreset(text ? std::string(text) : std::string());
				exported = path.empty() ? std::string("The clipboard doesn't hold a preset: copy one with Copy my settings first.") :
				                          std::format("Pasted and in use, saved as {}. Save settings keeps it.", path.filename().string());
			}
			Hint("A preset changes every setting at once, and nothing is kept until you press Save settings.\nIt never switches the widget "
				 "on or off.\nPresets live in Data/SKSE/Plugins/NeedsWidget/Presets/.");
			if (!exported.empty()) {
				FamilyUI::Dim(exported.c_str());
			}
		}

		// Words from a file name: dashes and underscores to spaces, the first letter a capital.
		std::string Words(std::string a_text)
		{
			std::ranges::replace(a_text, '_', ' ');
			std::ranges::replace(a_text, '-', ' ');
			if (!a_text.empty()) {
				a_text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(a_text[0])));
			}
			return a_text;
		}

		// The pictures group by icon (Tom, TEST31: "I like the grouping but it looks unformatted").
		// 3.0's alternatives are named icon first ("hunger_ham-shank"); an icon's own picture is
		// the bare name ("hunger"). The group is what comes before the _.
		std::string PictureGroup(const std::string& a_file)
		{
			return a_file.substr(0, a_file.find('_'));
		}

		// The group's heading: the icon's name as the menu shows it elsewhere.
		std::string GroupName(const std::string& a_group)
		{
			static const std::map<std::string, std::string> NAMES{ { "magicdebuffs", "Magic Debuffs" }, { "drum", "Bard - Drum" },
				{ "flute", "Bard - Flute" }, { "lute", "Bard - Lute" }, { "light", "Light Level" }, { "lantern", "Lantern Fuel" },
				{ "dirt", "Dirt and Blood" } };
			const auto it = NAMES.find(a_group);
			return it != NAMES.end() ? it->second : Words(a_group);
		}

		// The picture within its group: "Ham shank", or "Original" for the icon's own.
		std::string PictureShort(const std::string& a_file)
		{
			const auto under = a_file.find('_');
			if (under == std::string::npos) {
				return "Original";
			}
			// game-icons.net's own names, where they read badly as words.
			static const std::map<std::string, std::string> BETTER{ { "agility_wingfoot", "Winged foot" },
				{ "cold_thermometer-cold", "Cold thermometer" }, { "cold_tension-snowflake", "Spiky snowflake" },
				{ "dirt_empty-wood-bucket", "Empty bucket" }, { "dirt_full-wood-bucket", "Full bucket" },
				{ "disease_skull-with-syringe", "Skull and syringe" }, { "encumbrance_weight-crush", "Crushed by a weight" },
				{ "encumbrance_weight-scale", "Scales" }, { "hunger_knife-fork", "Knife and fork" },
				{ "intelligence_scroll-unfurled", "Open scroll" }, { "light_candlebright", "Bright candle" },
				{ "strength_muscle-up", "Flexed arm" }, { "strength_weight-lifting-up", "Weight lifting" },
				{ "temperature_thermometer-cold", "Cold thermometer" }, { "temperature_thermometer-hot", "Hot thermometer" },
				{ "thirst_mouth-watering", "Watering mouth" }, { "thirst_round-bottom-flask", "Round flask" },
				{ "vampire_fangs-circle", "Ring of fangs" }, { "wetness_splash-2", "Splash (other)" },
				{ "wounds_bloody-stash", "Bloody slash" } };
			if (const auto it = BETTER.find(a_file); it != BETTER.end()) {
				return it->second;
			}
			auto out = Words(a_file.substr(under + 1));
			if (out.size() > 2 && out.ends_with(" 1")) {
				out.resize(out.size() - 2);  // game-icons.net's "snowflake-1": the only one we ship
			}
			return out;
		}

		// "Hunger: Ham shank": the closed list and the Changed note.
		std::string PictureName(const std::string& a_file)
		{
			return std::format("{}: {}", GroupName(PictureGroup(a_file)), PictureShort(a_file));
		}

		std::string Lowered(std::string a_text)
		{
			for (auto& c : a_text) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return a_text;
		}

		// A search box at the top of an open list (Tom, TEST27-28: Style and Picture). Cleared and
		// focused each time the list opens; gives back what to look for, in lower case.
		std::string ListSearch(char* a_text, std::size_t a_size, const char* a_hint)
		{
			if (ImGui::IsWindowAppearing()) {
				a_text[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##find", a_hint, a_text, a_size);
			return Lowered(a_text);
		}

		// 3.0 (Tom, 2026-11-23: "LOADS of choice"): the four built-in looks, then every icon style in
		// NeedsWidget/Styles/. Shows on the widget at once, like any setting.
		bool StyleRow(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			static constexpr std::array<const char*, 4> LOOKS{ "Ring", "Bar", "Icon only", "Fill" };
			static constexpr const char* HELP =
				"How every icon looks: its frame, its meter and its colours. It changes on screen as you pick.\n"
				"Ring, Bar, Icon only and Fill are the classic looks. The rest, A to Z, are styles made to suit\n"
				"a UI mod or a part of Skyrim; type in the list's search box to find one, hover one to read\n"
				"about it. Styles are the files in\n"
				"Data/SKSE/Plugins/NeedsWidget/Styles/: a mod can add more, then press Look again.";
			const auto styles = Styles::List();
			const auto name = [&](const Settings::General& a_g) {
				if (const auto s = Styles::Find(a_g.style)) {
					return s->name;
				}
				return std::string(LOOKS[static_cast<std::size_t>(std::clamp(static_cast<int>(a_g.look), 0, 3))]);
			};
			const bool  differs = g.style != d.style || g.look != d.look;
			std::string words = std::format("Style look meter frame ring bar icon only fill {}", HELP);
			for (const auto& s : styles) {
				words += ' ';
				words += s.name;
			}
			bool changed = false;
			if (!a_rows.Line(words, differs)) {
				return false;
			}
			const auto note = std::format("Changed. Default: {}", name(d));
			ImGui::PushID("Style");
			FamilyUI::RowBegin(0);
			const bool hovered = FamilyUI::RowName("Style", false, differs ? note.c_str() : nullptr);
			ImGui::TableNextColumn();
			ImVec2 avail{};
			ImGui::GetContentRegionAvail(&avail);
			ImVec2 again{};
			ImGui::CalcTextSize(&again, "Look again", nullptr, true, -1.0f);
			ImGui::SetNextItemWidth((std::max)(avail.x - again.x - ImGui::GetStyle()->FramePadding.x * 2.0f - ImGui::GetStyle()->ItemSpacing.x, 60.0f));
			const bool styled = Styles::Find(g.style).has_value();
			if (ImGui::BeginCombo("##value", name(g).c_str(), ImGuiComboFlags_HeightLarge)) {
				// A search box at the top of the list (Tom, TEST27): typing narrows it by name or note.
				static char find[64]{};
				const auto  wanted = ListSearch(find, sizeof find, "Search styles");
				const auto  matches = [&](const std::string& a_name, const std::string& a_note) {
					return wanted.empty() || Lowered(a_name).find(wanted) != std::string::npos || Lowered(a_note).find(wanted) != std::string::npos;
				};
				int shown = 0;
				for (int k = 0; k < 4; ++k) {
					if (!matches(LOOKS[static_cast<std::size_t>(k)], "classic")) {
						continue;
					}
					++shown;
					if (ImGui::Selectable(std::format("{}##look{}", LOOKS[static_cast<std::size_t>(k)], k).c_str(), !styled && static_cast<int>(g.look) == k)) {
						g.look = static_cast<Settings::Look>(k);
						g.style.clear();
						changed = true;
					}
				}
				if (shown > 0 && wanted.empty()) {
					ImGui::Separator();
				}
				for (const auto& s : styles) {
					if (!matches(s.name, s.note)) {
						continue;
					}
					++shown;
					if (ImGui::Selectable(std::format("{}##{}", s.name, s.key).c_str(), styled && s.key == g.style)) {
						g.style = s.key;
						changed = true;
					}
					if (!s.note.empty() && ImGui::IsItemHovered()) {
						ImGui::SetTooltip("%s", s.note.c_str());
					}
				}
				if (shown == 0) {
					FamilyUI::Dim("No style has that word.");
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Look again")) {
				Styles::Scan();
				changed = true;  // the widget picks up a changed file at once
			}
			if (FamilyUI::RowReset(differs)) {
				g.look = d.look;
				g.style = d.style;
				changed = true;
			}
			HelpTip(hovered, "Style", HELP);
			FamilyUI::RowEnd();
			ImGui::PopID();
			if (!g.style.empty() && !styled && a_rows.Aside()) {
				FamilyUI::Dim(std::format("The style \"{}\" isn't in NeedsWidget/Styles/, so {} shows instead.", g.style,
					LOOKS[static_cast<std::size_t>(std::clamp(static_cast<int>(g.look), 0, 3))]).c_str());
			}
			return changed;
		}

		bool Look(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			auto colors = static_cast<int>(g.colors);
			// 3.0 (Fable, Tom): the style made for the UI mod in the game, offered once.
			if (!g.styleOffered && a_rows.Aside()) {
				static const auto found = Styles::Detected();
				if (found && found->key != g.style) {
					FamilyUI::Dim(std::format("Your game has the UI mod the {} style was made for.", found->name).c_str());
					if (ImGui::Button(std::format("Use {}", found->name).c_str())) {
						g.style = found->key;
						changed = true;
						Settings::SaveStyleOffered();
						g.styleOffered = true;
					}
					ImGui::SameLine();
					if (ImGui::Button("No thanks")) {
						Settings::SaveStyleOffered();
						g.styleOffered = true;
					}
				}
			}
			changed |= StyleRow(a_rows, g, d);
			changed |= a_rows.Toggle("Colour the icons as needs get worse", nullptr, &g.tintArt, d.tintArt);
			changed |= a_rows.Toggle("Meters drain as needs get worse",
				"Off: a meter fills up as a need gets worse. On: it starts full and empties,\nand the percent counts down with it.", &g.meterDrains,
				d.meterDrains);
			changed |= a_rows.Pick("Colours", "The style's own: each style's colours (Bone to red for the classic looks).", &colors,
				static_cast<int>(d.colors), { "Bone to red", "Green to red", "Custom", "The style's own", "Blue to orange (colour-blind)" });
			if (colors == 2) {
				changed |= CustomColors(a_rows, g.customColors, d.customColors);
			}
			if (const auto s = Styles::Find(g.style); s && s->ownColors && colors != 2 && a_rows.Aside()) {
				FamilyUI::Dim(std::format("{} always uses its own colours: others wouldn't show on it.", s->name).c_str());
			}
			changed |= a_rows.Toggle("Pulse an icon while that need is at its worst", nullptr, &g.pulseWorst, d.pulseWorst);
			changed |= a_rows.Toggle("Flash an icon when its need gets worse", nullptr, &g.flashWorse, d.flashWorse);
			changed |= a_rows.Toggle("Flash an icon when its need gets better", "A softer flash: the food, the bath or the fire worked.", &g.flashBetter, d.flashBetter);
			changed |= a_rows.Toggle("Stage pips under each icon", "Five small marks, one lit per stage, so the stage reads by count as well as colour.\nGood with Blue to orange.",
				&g.stagePips, d.stagePips);
			changed |= a_rows.Toggle("Trend arrow", "A small arrow by each icon: up while the need is getting worse, down while it gets better.\nIt shows for a few seconds each time the need moves.", &g.trend, d.trend);
			g.colors = static_cast<Settings::ColorScheme>(colors);
			return changed;
		}

		// Whether two orders put the icons there are in the same places (empty addon slots don't count).
		bool SameOrder(const std::array<int, Needs::kCount>& a_one, const std::array<int, Needs::kCount>& a_other)
		{
			const auto used = [](int a_id) { return Needs::Used(static_cast<Needs::Id>(a_id)); };
			return std::ranges::equal(a_one | std::views::filter(used), a_other | std::views::filter(used));
		}

		bool Layout(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			auto arrangement = static_cast<int>(g.arrangement);
			auto farEnd = g.fromFarEnd ? 1 : 0;
			changed |= a_rows.Pick("Arrangement", "Row and Column line the icons up by themselves.\nFree: place each icon yourself, on the Icons page.",
				&arrangement, static_cast<int>(d.arrangement), { "Row", "Column", "Free" });
			changed |= a_rows.Slider("Size of all icons", "Scales the icons and the gaps between them.", &g.groupSize, d.groupSize, 25.0f, 300.0f,
				"%.0f%%");
			if (arrangement != 2) {
				const bool row = arrangement == 0;
				static constexpr std::array<const char*, 2> ROW_ENDS{ "The left", "The right" };
				static constexpr std::array<const char*, 2> COLUMN_ENDS{ "The top", "The bottom" };
				static constexpr std::array<const char*, 2> ROW_WRAPS{ "Above", "Below" };
				static constexpr std::array<const char*, 2> COLUMN_WRAPS{ "Left", "Right" };
				static constexpr const char*                POSITION = "The position is the icon at the end you start from.";
				changed |= a_rows.Pick("Start from", POSITION, &farEnd, d.fromFarEnd ? 1 : 0, row ? ROW_ENDS : COLUMN_ENDS);
				changed |= a_rows.Slider("Position by X", POSITION, &g.lineX, d.lineX, 0.0f, 100.0f, "%.1f%%");
				changed |= a_rows.Slider("Position by Y", POSITION, &g.lineY, d.lineY, 0.0f, 100.0f, "%.1f%%");
				changed |= a_rows.Slider("Spacing", "The gap between icons, as a share of an icon's size.", &g.spacing, d.spacing, 0.0f, 200.0f,
					"%.0f%%");
				// A float slider, not SliderInt: the dll only calls what SMF exports.
				float perLine = static_cast<float>(g.perLine);
				changed |= a_rows.Slider(row ? "Icons per row" : "Icons per column", row ? "Any more start a new row." : "Any more start a new column.",
					&perLine, static_cast<float>(d.perLine), 0.0f, static_cast<float>(Needs::kCount), g.perLine == 0 ? "No limit" : "%.0f",
					d.perLine == 0 ? "No limit" : "%.0f");
				g.perLine = static_cast<int>(perLine + 0.5f);
				if (g.perLine > 0) {
					auto after = g.wrapBefore ? 0 : 1;
					changed |= a_rows.Pick(row ? "New rows go" : "New columns go", nullptr, &after, d.wrapBefore ? 0 : 1, row ? ROW_WRAPS : COLUMN_WRAPS);
					g.wrapBefore = after == 0;
					changed |= a_rows.Slider(row ? "Gap between rows" : "Gap between columns", "A share of the biggest icon's size.", &g.lineGap,
						d.lineGap, 0.0f, 200.0f, "%.0f%%");
				}
				changed |= a_rows.Toggle("Close the gaps when an icon is hidden", nullptr, &g.closeGaps, d.closeGaps);
				changed |= a_rows.Toggle("Worst first", "The icons line up by how bad each need is, the worst at the end you start from.\nThe order below breaks ties.",
					&g.worstFirst, d.worstFirst);
				static constexpr const char* SECOND = "Icons put in the second row (each icon's section, Row) line up here instead,\nthe same way as the first.";
				changed |= a_rows.Slider(row ? "Second row: position by X" : "Second column: position by X", SECOND, &g.line2X, d.line2X, 0.0f, 100.0f, "%.1f%%");
				changed |= a_rows.Slider(row ? "Second row: position by Y" : "Second column: position by Y", SECOND, &g.line2Y, d.line2Y, 0.0f, 100.0f, "%.1f%%");

				const bool reordered = !SameOrder(g.order, d.order);
				if (a_rows.Block("order of the icons up down first last", reordered)) {
					ImGui::PushStyleColor(ImGuiCol_Text, FamilyUI::Accent());
					ImGui::SeparatorText(row ? "Order, left to right" : "Order, top to bottom");
					ImGui::PopStyleColor();
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
					if (reordered) {
						ImGui::TextColored(kChanged, "%s", "Changed.");
						ImGui::SameLine();
						if (ImGui::SmallButton("Reset##order")) {
							g.order = d.order;
							changed = true;
						}
						FamilyUI::Tip("Reset", "Puts the icons back in the default order.");
					}
				}
			} else {
				static constexpr const char* MOVE = "Moves every icon together. Each icon's own section still moves it within the group.";
				changed |= a_rows.Slider("Move all by X", MOVE, &g.groupX, d.groupX, -100.0f, 100.0f, "%.1f%%");
				changed |= a_rows.Slider("Move all by Y", MOVE, &g.groupY, d.groupY, -100.0f, 100.0f, "%.1f%%");
			}
			{
				float most = static_cast<float>(g.maxShown);
				changed |= a_rows.Slider("Show at most", "Only this many icons show: the worst ones. Ties go by the row's order.", &most, static_cast<float>(d.maxShown), 0.0f,
					static_cast<float>(Needs::kCount), g.maxShown == 0 ? "All" : "%.0f", d.maxShown == 0 ? "All" : "%.0f");
				g.maxShown = static_cast<int>(most + 0.5f);
			}
			changed |= a_rows.Slider("Grow at the worst two stages", "Icons at their worst two stages get this much bigger. 0: never.", &g.growBad, d.growBad, 0.0f,
				100.0f, g.growBad == 0.0f ? "Never" : "%.0f%%", d.growBad == 0.0f ? "Never" : "%.0f%%");
			g.arrangement = static_cast<Settings::Arrangement>(arrangement);
			g.fromFarEnd = farEnd == 1;
			return changed;
		}

		bool Numbers(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			if (a_rows.Block("show every number hide every number", false, false)) {
				if (ImGui::Button("Show every number")) {
					Settings::ShowAllNumbers(true);
				}
				ImGui::SameLine();
				if (ImGui::Button("Hide every number")) {
					Settings::ShowAllNumbers(false);
				}
				Hint("Each icon's section on the Icons page can still turn its own number on or off.");
			}
			auto above = g.numbersAbove ? 1 : 0;
			changed |= a_rows.Pick("Numbers go", nullptr, &above, d.numbersAbove ? 1 : 0, { "Under the icons", "Above the icons" });
			changed |= a_rows.Pick("Numbers show", "Time to worst: game hours until the need reaches its worst stage, at the rate of the last\nminute. A dash while it isn't getting worse.",
				&g.numberShows, d.numberShows, { "The percent", "The stage's name", "Time to worst" });
			changed |= a_rows.Slider("Number size", "On top of the icons' own size.", &g.numberSize, d.numberSize, 50.0f, 400.0f, "%.0f%%");
			changed |= a_rows.Slider("Distance from the icon", "How far under or above the icon the number sits.", &g.numberGap, d.numberGap, -60.0f,
				60.0f, "%.0f");
			changed |= a_rows.Slider("Move sideways", "Moves every number left or right of its icon.", &g.numberX, d.numberX, -200.0f, 200.0f, "%.0f");
			changed |= a_rows.Pick("Line up",
				"For a column: pick Start at the icon, move the text right with Move sideways and\nup beside the icon with Distance from the icon. "
				"The names then line up on their left edge.",
				&g.numberAlign, d.numberAlign, { "Centred", "Start at the icon", "End at the icon" });
			changed |= a_rows.Toggle("Colour the numbers like their icon", nullptr, &g.numberTint, d.numberTint);
			changed |= a_rows.Toggle("Dark edge round the numbers", "Easier to read over snow and sky.", &g.numberEdge, d.numberEdge);
			static constexpr const char* FONTS[] = { "Medium (default)", "Bold", "Light", "Plain", "Book", "Handwritten" };
			static constexpr const char* FONT_HELP =
				"The game's own fonts, so a font mod changes these too. Plain is the console's\nfont, usually the clearest. Bold is easier to read "
				"than Medium at small sizes.";
			if (const bool differs = g.numberFont != d.numberFont;
				a_rows.Line(std::format("Font {} medium bold light plain book handwritten", FONT_HELP), differs)) {
				const auto note = std::format("Changed. Default: {}", FONTS[std::clamp(d.numberFont, 0, static_cast<int>(std::size(FONTS)) - 1)]);
				ImGui::PushID("Font");
				FamilyUI::RowBegin(0);
				const bool hovered = FamilyUI::RowName("Font", false, differs ? note.c_str() : nullptr);
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed |= ImGui::Combo("##value", &g.numberFont, FONTS, static_cast<int>(std::size(FONTS)));
				if (FamilyUI::RowReset(differs)) {
					g.numberFont = d.numberFont;
					changed = true;
				}
				HelpTip(hovered, "Font", FONT_HELP);
				FamilyUI::RowEnd();
				ImGui::PopID();
			}
			changed |= a_rows.Pick("Letters",
				"For stage names. Some fonts only have capitals, so lower case shows no change in those:\ntry Plain or Book with it.", &g.numberCase,
				d.numberCase, { "As written", "lower case", "UPPER CASE" });
			g.numbersAbove = above == 1;
			return changed;
		}

		bool Fading(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			changed |= a_rows.Toggle("Fade out when nothing has changed for a while",
				"Any change brings the icons back: a need getting better or worse, or an icon coming or going.", &g.fadeWhenStill, d.fadeWhenStill);
			if (g.fadeWhenStill) {
				changed |= a_rows.Slider("Fade out after", "How long nothing has to change before the icons fade out.", &g.fadeAfter, d.fadeAfter, 2.0f,
					120.0f, "%.0f seconds");
				changed |= a_rows.Toggle("Stay on screen while a need is at its worst two stages", nullptr, &g.keepBad, d.keepBad);
			}
			return changed;
		}

		// 3.0 (Fable, Tom: "add them all"): when the widget stays out of the way.
		bool WhenToShow(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			changed |= a_rows.Toggle("Hide in combat", nullptr, &g.hideInCombat, d.hideInCombat);
			changed |= a_rows.Toggle("Hide with a weapon or spell out", nullptr, &g.hideWeaponDrawn, d.hideWeaponDrawn);
			changed |= a_rows.Toggle("Hide while riding", nullptr, &g.hideMounted, d.hideMounted);
			if (a_rows.Aside()) {
				FamilyUI::Dim("Each icon's section can also show it only outdoors or only indoors.");
			}
			return changed;
		}

		// The key being picked: read here once a frame, whichever group is open.
		bool PollHotkey(Settings::General& g)
		{
			if (Hotkey::Capturing()) {
				for (int k = ImGuiKey_NamedKey_BEGIN; k <= ImGuiKey_KeypadEqual; ++k) {
					if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k), false)) {
						Hotkey::Offer(k);
					}
				}
			}
			if (const int key = Hotkey::TakeCaptured(); key > 0) {
				g.hotkey = key;
				return true;
			}
			return false;
		}

		bool HotkeySection(Rows& a_rows, Settings::General& g, const Settings::General& d)
		{
			bool changed = false;
			static constexpr const char* KEY_HELP =
				"Press the button, then the key you want. Esc keeps the old one. SKSE Menu Framework's own key can't be used.";
			if (const bool differs = g.hotkey != d.hotkey; a_rows.Line(std::format("Key hotkey press no key {}", KEY_HELP), differs)) {
				const auto note = std::format("Changed. Default: {}", d.hotkey > 0 ? Hotkey::KeyName(d.hotkey) : std::string("none"));
				ImGui::PushID("Key");
				FamilyUI::RowBegin(0);
				bool hovered = FamilyUI::RowName("Key", false, differs ? note.c_str() : nullptr);
				ImGui::TableNextColumn();
				const auto keyText = Hotkey::Capturing() ? std::string("Press a key (Esc: keep the old one)###key") :
				                                           std::format("{}###key", g.hotkey > 0 ? Hotkey::KeyName(g.hotkey) : std::string("None: press to pick one"));
				if (ImGui::Button(keyText.c_str())) {
					Hotkey::StartCapture();
				}
				hovered = hovered || ImGui::IsItemHovered();
				if (g.hotkey > 0) {
					ImGui::SameLine();
					if (ImGui::Button("No key")) {
						g.hotkey = 0;
						changed = true;
					}
				}
				if (Hotkey::Capturing() && Hotkey::Refused()) {
					FamilyUI::Wrapped(kWarning, "That's SKSE Menu Framework's key: press another.");
				}
				if (FamilyUI::RowReset(differs)) {
					g.hotkey = d.hotkey;
					changed = true;
				}
				HelpTip(hovered, "Key", KEY_HELP);
				FamilyUI::RowEnd();
				ImGui::PopID();
			}
			changed |= a_rows.Pick("The key", "Hides and shows: flips Show the widget on this page, kept after you quit.\nBrings it back: only matters with fading on, the widget is always up otherwise.", &g.hotkeyMode,
				d.hotkeyMode, { "Hides and shows the widget", "Brings it back for a while" });
			return changed;
		}

		struct GeneralGroup
		{
			const char* id;
			const char* label;
			bool (*lines)(Rows&, Settings::General&, const Settings::General&);
		};
		constexpr std::array GENERAL_GROUPS{
			GeneralGroup{ "look", "Look", Look },
			GeneralGroup{ "layout", "Layout", Layout },
			GeneralGroup{ "numbers", "Numbers", Numbers },
			GeneralGroup{ "fading", "Fading", Fading },
			GeneralGroup{ "when", "When to show", WhenToShow },
			GeneralGroup{ "hotkey", "Hotkey", HotkeySection },
		};

		// General's settings: a bar per group that folds, with its count. While searching, only
		// the lines that hold the word, under their group's name.
		void GeneralGroups()
		{
			auto       g = Settings::GetGeneral();
			const auto d = Settings::GetGeneralDefaults();
			const bool searching = search.Active();
			bool       changed = PollHotkey(g);
			{
				const auto presets = Settings::GetPresets();
				bool       open = searching;
				if (!searching) {
					FamilyUI::Group group{ "Presets" };
					group.badge = FamilyUI::CountBadge(presets.size() + 1, "preset");
					group.openFirst = true;
					open = FamilyUI::GroupBar("presets", group).open;
				}
				if (open) {
					ImGui::PushID("presets lines");
					{
						Rows rows("Presets", Rows::Mode::kDraw);
						Presets(rows, presets);
					}
					// A preset or Default may have just replaced General: the groups below
					// start from that, or a change of theirs this frame would put the old one back.
					const auto key = g.hotkey;
					g = Settings::GetGeneral();
					if (changed) {
						g.hotkey = key;
					}
					ImGui::PopID();
					if (!searching) {
						ImGui::Spacing();
					}
				}
			}
			for (const auto& def : GENERAL_GROUPS) {
				bool open = searching;
				if (!searching) {
					auto copy = g;
					Rows counted(def.label, Rows::Mode::kCount);
					def.lines(counted, copy, d);
					FamilyUI::Group group{ def.label };
					Badge(group, counted);
					open = FamilyUI::GroupBar(def.id, group).open;
				}
				if (open) {
					ImGui::PushID(def.label);
					{
						Rows rows(def.label, Rows::Mode::kDraw);
						changed |= def.lines(rows, g, d);
					}
					ImGui::PopID();
					if (!searching) {
						ImGui::Spacing();
					}
				}
			}
			if (changed) {
				Settings::SetGeneral(g);
			}
		}

		// Every picture the widget can show: Data/Interface/NeedsWidget/icons/*.swf, but the frames.
		// Read when a page first asks, and again from the button (3.0, Tom: pick an icon's picture).
		std::vector<std::string> pictures;
		bool                     picturesRead = false;

		const std::vector<std::string>& Pictures(bool a_again = false)
		{
			if (picturesRead && !a_again) {
				return pictures;
			}
			picturesRead = true;
			pictures.clear();
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator("Data/Interface/NeedsWidget/icons", ec)) {
				const auto& path = entry.path();
				auto        ext = path.extension().string();
				std::ranges::transform(ext, ext.begin(), [](unsigned char a_c) { return static_cast<char>(std::tolower(a_c)); });
				const auto stem = path.stem().string();
				if (ext == ".swf" && !stem.starts_with("frame_") && std::ranges::find(pictures, stem) == pictures.end()) {
					pictures.push_back(stem);
				}
			}
			// By group A to Z, each group's Original first, then A to Z (Tom, TEST28 and TEST31).
			std::ranges::sort(pictures, [](const std::string& a, const std::string& b) {
				const auto ga = Lowered(GroupName(PictureGroup(a)));
				const auto gb = Lowered(GroupName(PictureGroup(b)));
				if (ga != gb) {
					return ga < gb;
				}
				const bool oa = a.find('_') == std::string::npos;
				const bool ob = b.find('_') == std::string::npos;
				if (oa != ob) {
					return oa;
				}
				return Lowered(PictureShort(a)) < Lowered(PictureShort(b));
			});
			logs::info("{} pictures to pick from in Interface/NeedsWidget/icons{}", pictures.size(), ec ? std::format(" ({})", ec.message()) : "");
			return pictures;
		}

		// One icon's lines. a_shown: Show this icon was changed, which keeps itself (Settings::SaveShown).
		bool IconLines(Rows& a_rows, Needs::Id a_id, Settings::Icon& icon, const Settings::Icon& d, bool& a_shown)
		{
			bool changed = false;
			if (a_rows.Aside()) {
				const auto r = Needs::Last(a_id);
				if (r.available) {
					ImGui::TextColored(kDim, "Now: %s, %.0f%%, from %s", r.stageName ? r.stageName : "", r.fraction * 100.0f, r.source);
				} else {
					ImGui::TextColored(kDim, "%s", Needs::WhyHidden(a_id).c_str());
					Hint("While this menu is open it shows faded, so you can still place it.");
				}
			}
			// 2.9.0 (Brunny): with two mods for one need, pick the one to read.
			if (const auto mods = Needs::SourceMods(a_id); mods.size() > 1) {
				static constexpr const char* HELP = "Automatic uses the first of these mods that's running, top to bottom.\nPick one to read only that mod.";
				const bool                   differs = icon.source != d.source;
				std::string                  words = std::format("Read from automatic mod {}", HELP);
				for (const auto& m : mods) {
					words += ' ';
					words += m;
				}
				if (a_rows.Line(words, differs)) {
					const auto note = std::format("Changed. Default: {}", d.source.empty() ? std::string("Automatic") : d.source);
					ImGui::PushID("Read from");
					FamilyUI::RowBegin(0);
					const bool hovered = FamilyUI::RowName("Read from", false, differs ? note.c_str() : nullptr);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					const std::string current = icon.source.empty() ? std::string("Automatic (the first one running)") : icon.source;
					if (ImGui::BeginCombo("##value", current.c_str())) {
						if (ImGui::Selectable("Automatic (the first one running)", icon.source.empty())) {
							icon.source.clear();
							changed = true;
						}
						for (const auto& m : mods) {
							if (ImGui::Selectable(m.c_str(), icon.source == m)) {
								icon.source = m;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
					if (FamilyUI::RowReset(differs)) {
						icon.source = d.source;
						changed = true;
					}
					HelpTip(hovered, "Read from", HELP);
					FamilyUI::RowEnd();
					ImGui::PopID();
				}
			}
			if (a_rows.Toggle("Show this icon", "Off hides this icon. Kept at once: this one needs no Save settings.", &icon.enabled, d.enabled, true)) {
				changed = true;
				a_shown = true;
			}
			// 3.0 (GenieA, Tom: a choice): one icon per disease, Disease 2-6 beside this one.
			if (a_id == Needs::kDisease) {
				auto g = Settings::GetGeneral();
				if (a_rows.Toggle("One icon per disease",
						"On: this icon shows the worst disease, and Disease 2 to Disease 6 the next ones,\n"
						"each with its own section below. Off: this icon names the worst, with (+N) for the rest.",
						&g.diseaseEach, Settings::GetGeneralDefaults().diseaseEach)) {
					Settings::SetGeneral(g);
				}
			}
			changed |= a_rows.Toggle("Show the number", nullptr, &icon.showNumber, d.showNumber);
			if (icon.showNumber) {
				static constexpr const char* HELP = "On top of General > Numbers, for this icon only.";
				changed |= a_rows.Slider("Move this number sideways", HELP, &icon.numberX, d.numberX, -200.0f, 200.0f, "%.0f");
				changed |= a_rows.Slider("Move this number up or down", HELP, &icon.numberY, d.numberY, -120.0f, 120.0f, "%.0f");
			}
			{
				const auto label = a_rows.Counting() ? std::string("Hide when fine") : std::format("Hide when fine (fine = {})", Needs::FineMeaning(a_id));
				changed |= a_rows.Toggle(label.c_str(), nullptr, &icon.hideWhenFine, d.hideWhenFine);
			}
			// 2.10 (HardSignage: "the Fine number ... doesn't seem to be configurable"): where fine ends.
			if (icon.hideWhenFine) {
				static constexpr const char* HELP = "Hide when fine hides this icon at this stage and better.\nThe mod's own stages, best first.";
				const auto names = Needs::StageNames(a_id);
				const int  own = Needs::DefaultFineUpTo(a_id);
				const int  now = icon.fineUpTo >= 0 ? icon.fineUpTo : own;
				const bool differs = icon.fineUpTo != d.fineUpTo;
				const auto stage = [&](int a_k) {
					return std::format("{}. {}", a_k + 1, names[static_cast<std::size_t>(a_k)] ? names[static_cast<std::size_t>(a_k)] : "");
				};
				if (a_rows.Line(std::format("Counts as fine up to stage hide when fine {}", HELP), differs)) {
					const auto note = std::format("Changed. Default: {}", stage(std::clamp(own, 0, 4)));
					ImGui::PushID("Fine up to");
					FamilyUI::RowBegin(0);
					const bool hovered = FamilyUI::RowName("Counts as fine up to", false, differs ? note.c_str() : nullptr);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					if (ImGui::BeginCombo("##value", stage(std::clamp(now, 0, 4)).c_str())) {
						for (int k = 0; k <= 4; ++k) {
							if (ImGui::Selectable(std::format("{}{}", stage(k), k == own ? " (the icon's own)" : "").c_str(), k == now)) {
								icon.fineUpTo = k == own ? -1 : k;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
					if (FamilyUI::RowReset(differs)) {
						icon.fineUpTo = d.fineUpTo;
						changed = true;
					}
					HelpTip(hovered, "Counts as fine up to", HELP);
					FamilyUI::RowEnd();
					ImGui::PopID();
				}
			}
			if (Needs::HideFrom(a_id) >= 0) {
				const auto label = Needs::HideFromText(a_id);
				changed |= a_rows.Toggle(label.c_str(), nullptr, &icon.hideFromStage, d.hideFromStage);
			}
			// 3.0 (Tom): any picture in the icons folder, for any icon. Shows on the widget at once.
			{
				static constexpr const char* HELP =
					"Any picture the widget has, for this icon. It changes on screen as you pick.\n"
					"Grouped by icon, this one first, with a search box: the mod page shows them all by name.\n"
					"Pictures are the .swf files in Data/Interface/NeedsWidget/icons/: a reskin or\n"
					"another mod can add more, then press Look again.";
				const auto own = Needs::DefaultArt(a_id);
				const auto now = icon.art.empty() ? own : icon.art;
				const bool differs = icon.art != d.art;
				std::string words = std::format("Picture icon art glyph image look {}", HELP);
				if (a_rows.Line(words, differs)) {
					const auto note = std::format("Changed. Default: {}", PictureName(d.art.empty() ? own : d.art));
					ImGui::PushID("Picture");
					FamilyUI::RowBegin(0);
					const bool hovered = FamilyUI::RowName("Picture", false, differs ? note.c_str() : nullptr);
					ImGui::TableNextColumn();
					ImVec2 avail{};
					ImGui::GetContentRegionAvail(&avail);
					ImVec2 again{};
					ImGui::CalcTextSize(&again, "Look again", nullptr, true, -1.0f);
					ImGui::SetNextItemWidth((std::max)(avail.x - again.x - ImGui::GetStyle()->FramePadding.x * 2.0f - ImGui::GetStyle()->ItemSpacing.x, 60.0f));
					if (ImGui::BeginCombo("##value", PictureName(now).c_str(), ImGuiComboFlags_HeightLarge)) {
						static char find[64]{};
						const auto  wanted = ListSearch(find, sizeof find, "Search pictures");
						int         shown = 0;
						// This icon's own group first, then the rest A to Z, each under its heading.
						const auto ownGroup = PictureGroup(own);
						for (const bool first : { true, false }) {
							std::string heading;
							for (const auto& file : Pictures()) {
								if ((PictureGroup(file) == ownGroup) != first) {
									continue;
								}
								if (!wanted.empty() && Lowered(PictureName(file)).find(wanted) == std::string::npos) {
									continue;
								}
								++shown;
								if (const auto group = GroupName(PictureGroup(file)); group != heading) {
									heading = group;
									ImGui::SeparatorText(heading.c_str());
								}
								const auto label = std::format("   {}{}##{}", PictureShort(file), file == own ? "  (default)" : "", file);
								if (ImGui::Selectable(label.c_str(), file == now)) {
									icon.art = file == own ? std::string() : file;
									changed = true;
								}
								if (file == now && ImGui::IsWindowAppearing()) {
									ImGui::SetScrollHereY(0.5f);
								}
							}
						}
						if (shown == 0) {
							FamilyUI::Dim("No picture has that word.");
						}
						ImGui::EndCombo();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Look again")) {
						Pictures(true);
					}
					if (FamilyUI::RowReset(differs)) {
						icon.art = d.art;
						changed = true;
					}
					HelpTip(hovered, "Picture", HELP);
					FamilyUI::RowEnd();
					ImGui::PopID();
				}
			}
			changed |= a_rows.Pick("Show", "Outdoors and indoors go by the cell you're in.", &icon.where, d.where, { "Anywhere", "Only outdoors", "Only indoors" });
			if (Settings::GetGeneral().arrangement != Settings::Arrangement::kFree) {
				changed |= a_rows.Pick("Line up in", "The second row or column is placed on the General page, Layout.", &icon.group, d.group,
					{ "The first row", "The second row" });
			}
			changed |= a_rows.Slider("Size", "This icon's own size, on top of Size of all icons.", &icon.size, d.size, 25.0f, 300.0f, "%.0f%%");
			changed |= a_rows.Slider("Opacity", "How solid this icon is. At 0% it can't be seen.", &icon.opacity, d.opacity, 0.0f, 100.0f, "%.0f%%");
			if (Settings::GetGeneral().arrangement == Settings::Arrangement::kFree) {
				static constexpr const char* HELP = "Where this icon sits, as a share of the screen.";
				changed |= a_rows.Slider("Position by X", HELP, &icon.x, d.x, 0.0f, 100.0f, "%.1f%%");
				changed |= a_rows.Slider("Position by Y", HELP, &icon.y, d.y, 0.0f, 100.0f, "%.1f%%");
			}
			return changed;
		}

		// One icon: a bar with its own Show this icon switch, its name and stage, and its count
		// (red and OFF while it's switched off). While searching, only its lines that hold the word.
		void IconGroup(Needs::Id a_id)
		{
			auto        icon = Settings::Get(a_id);
			const auto  d = Settings::GetDefaults(a_id);
			const auto  r = Needs::Last(a_id);
			const auto  label = r.available && r.stageName ? std::format("{} ({})", Needs::Name(a_id), r.stageName) : std::format("{} (hidden)", Needs::Name(a_id));
			const bool  searching = search.Active();
			bool        changed = false;
			bool        shown = false;
			bool        restore = false;
			bool        open = searching;
			ImGui::PushID(Needs::Name(a_id));
			if (!searching) {
				auto copy = icon;
				bool unused = false;
				Rows counted(label, Rows::Mode::kCount);
				IconLines(counted, a_id, copy, d, unused);
				bool            on = icon.enabled;
				FamilyUI::Group group{ label.c_str() };
				group.on = &on;
				group.switchTip = "Show this icon. Kept at once: this one needs no Save settings.";
				if (on) {
					Badge(group, counted);
				} else {
					group.badge = "OFF";
					group.badgeColour = kOff;
				}
				const auto state = FamilyUI::GroupBar("need", group);
				if (state.toggled) {
					icon.enabled = on;
					changed = true;
					shown = true;
				}
				open = state.open;
			}
			if (open) {
				{
					Rows rows(label, Rows::Mode::kDraw);
					changed |= IconLines(rows, a_id, icon, d, shown);
					if (rows.Aside()) {
						restore = ImGui::Button("Restore this icon's defaults");
					}
				}
				if (!searching) {
					ImGui::Spacing();
				}
			}
			ImGui::PopID();
			if (changed) {
				Settings::Set(a_id, icon);
			}
			if (shown) {
				saveFailed = !Settings::SaveShown(a_id);
			}
			if (restore) {
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

		// One group per need whose mod is in the game, in the row's order: a_page's, or every
		// page's when there is none (searching). Whether there were any.
		bool IconGroups(const std::string_view* a_page)
		{
			bool any = false;
			for (const auto id : Settings::GetGeneral().order) {
				const auto need = static_cast<Needs::Id>(id);
				if (!Needs::Installed(need) || (a_page && PageOf(need) != *a_page)) {
					continue;
				}
				any = true;
				IconGroup(need);
			}
			return any;
		}

		// A page's icons, when not searching.
		void IconSections(std::string_view a_page)
		{
			if (Settings::GetGeneral().arrangement != Settings::Arrangement::kFree) {
				ImGui::TextColored(kDim, "%s", "The row or column on the General page places the icons.");
				Hint("Pick Free under General > Layout to place each icon yourself.");
			}
			if (!IconGroups(&a_page)) {
				// An addon's page with none of its mods in the game.
				ImGui::TextColored(kDim, "%s", "None of the mods these icons read are in this game.");
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

		// While searching, on any page: every line that holds the word, from General and from every icon.
		void Found()
		{
			if (lastTally.needle == search.needle) {
				FamilyUI::SearchResult(lastTally.lines, lastTally.groups, "setting", ", from General and every icon");
			} else {
				ImGui::TextUnformatted("");
			}
			tally = { search.needle, 0, 0 };
			GeneralGroups();
			IconGroups(nullptr);
			lastTally = tally;
		}

		// The top of every page: its name, and what it holds.
		void Top(const char* a_title, const char* a_about)
		{
			Settings::MarkEditing();
			FamilyUI::Title(a_title, a_about);
		}

		// Where things stand, as tags.
		void StatusTags(bool a_general)
		{
			const auto                 g = Settings::GetGeneral();
			std::vector<FamilyUI::Tag> tags;
			if (!g.showWidget) {
				tags.push_back({ "Widget: off", kOff, "Widget", "Every icon is hidden. Show the widget, on the General page, brings them back." });
			}
			if (a_general) {
				static constexpr std::array<const char*, 3> LAYOUTS{ "Row", "Column", "Free" };
				static constexpr std::array<const char*, 4> METERS{ "Ring", "Bar", "Icon only", "Fill" };
				std::string                                 preset = Settings::MatchesDefaults() ? "Default" : "";
				for (const auto& p : Settings::GetPresets()) {
					if (preset.empty() && Settings::IsInUse(p)) {
						preset = p.name;
					}
				}
				tags.push_back({ std::format("Preset: {}", preset.empty() ? "your own" : preset), preset.empty() ? kChanged : FamilyUI::Accent(), "Preset",
					"The preset in use. Your own: you changed something since picking one." });
				tags.push_back({ std::format("Layout: {}", LAYOUTS[static_cast<std::size_t>(std::clamp(static_cast<int>(g.arrangement), 0, 2))]),
					FamilyUI::Accent(), "Layout", "Row and Column line the icons up by themselves. Free: you place each one." });
				const auto picked = Styles::Find(g.style);
				tags.push_back({ picked ? std::format("Style: {}", picked->name) : std::format("Meter: {}", METERS[static_cast<std::size_t>(std::clamp(static_cast<int>(g.look), 0, 3))]), FamilyUI::Accent(),
					"Style", "How every icon looks: its frame, its meter and its colours." });
			}
			int icons = 0;
			int shown = 0;
			for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
				const auto need = static_cast<Needs::Id>(id);
				if (Needs::Installed(need)) {
					++icons;
					shown += Settings::Get(need).enabled ? 1 : 0;
				}
			}
			tags.push_back({ std::format("Icons: {} of {} switched on", shown, icons), kDim, "Icons",
				"Each need with a mod in this game has an icon. Switch them on and off on the Icons page." });
			if (Settings::HasUnsavedChanges()) {
				tags.push_back({ "Not saved yet", kWarning, "Not saved yet", "Press Save settings, at the bottom, to keep your changes after you quit." });
			} else {
				tags.push_back({ "Saved", kGood, "Saved", "Nothing has changed since the last save." });
			}
			FamilyUI::TagRow(tags);
			ImGui::Spacing();
		}

		// The needs as they stand now, as small bars in the icons' own colours. Only what the
		// widget itself last read (Needs::Last): nothing is read from the game here.
		void LiveNeeds(const Settings::General& a_general)
		{
			struct Live
			{
				Needs::Id      id;
				Needs::Reading reading;
			};
			std::vector<Live> live;
			for (const auto id : a_general.order) {
				const auto need = static_cast<Needs::Id>(id);
				if (!Needs::Installed(need)) {
					continue;
				}
				if (const auto r = Needs::Last(need); r.available) {
					live.push_back({ need, r });
				}
			}
			if (live.empty()) {
				return;
			}
			FamilyUI::Section("Your needs now");
			ImVec2 avail{};
			ImGui::GetContentRegionAvail(&avail);
			const int columns = std::clamp(static_cast<int>(avail.x / (ImGui::GetFontSize() * 9.0f)), 1, 8);
			if (!ImGui::BeginTable("now", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
				return;
			}
			const auto  colours = Settings::StageColors(a_general);
			const float height = std::round(ImGui::GetFontSize() * 0.35f);
			auto*       draw = ImGui::GetWindowDrawList();
			for (const auto& [id, r] : live) {
				ImGui::TableNextColumn();
				const auto   c = colours[static_cast<std::size_t>(std::clamp(r.stage, 0, 5))];
				const ImVec4 colour{ ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f };
				ImGui::TextColored(kDim, "%s", Needs::Name(id));
				bool   hovered = ImGui::IsItemHovered();
				ImVec2 pos{};
				ImVec2 cell{};
				ImGui::GetCursorScreenPos(&pos);
				ImGui::GetContentRegionAvail(&cell);
				const float width = (std::max)(cell.x - 6.0f, 8.0f);
				float       share = std::clamp(r.fraction, 0.0f, 1.0f);
				if (a_general.meterDrains) {
					share = 1.0f - share;
				}
				ImGui::ImDrawListManager::AddRectFilled(draw, pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(FamilyUI::Fade(kDim, 0.25f)),
					2.0f, 0);
				if (share > 0.0f) {
					ImGui::ImDrawListManager::AddRectFilled(draw, pos, ImVec2(pos.x + (std::max)(width * share, 2.0f), pos.y + height),
						ImGui::GetColorU32(colour), 2.0f, 0);
				}
				ImGui::Dummy(ImVec2(width, height + 4.0f));
				hovered = hovered || ImGui::IsItemHovered();
				if (hovered && FamilyUI::TipBegin(Needs::Name(id))) {
					ImGui::Text("%s, %.0f%%, from %s", r.stageName ? r.stageName : "", r.fraction * 100.0f, r.source);
					FamilyUI::TipEnd();
				}
			}
			ImGui::EndTable();
			ImGui::Spacing();
		}

		void __stdcall RenderGeneral()
		{
			FamilyUI::Page page;
			Top("General", "The look, the layout, the numbers, fading and the key.");
			StatusTags(true);
			auto g = Settings::GetGeneral();
			if (bool on = g.showWidget; FamilyUI::MasterSwitch("Show the widget", &on, "The icons show while you play.",
					"Every icon is hidden. Your layout and settings are kept.",
					"The whole widget on or off. Off hides every icon and changes nothing else.\nKept at once: this one needs no Save "
					"settings. The hotkey, set to hide and show, flips this same switch.")) {
				saveFailed = !Settings::SaveShowWidget(on);
				g = Settings::GetGeneral();
			}
			LiveNeeds(g);
			if (FamilyUI::SearchBox(search, kSearchHint)) {
				Found();
			} else {
				ImGui::Spacing();
				DragButton();
				GeneralGroups();
			}
			SaveBar(Settings::RestoreGeneralDefaults, "Restore General's defaults");
		}

		// Its own tab too: Tom looked for it in the tabs first.
		void __stdcall RenderDragTab()
		{
			FamilyUI::Page page;
			Top("Drag icons", "Move and resize the icons with the mouse, right on the screen.");
			FamilyUI::Dim("This menu closes while you do. Done brings it back.");
			ImGui::Spacing();
			const bool on = Settings::GetGeneral().showWidget;
			if (dragWindow) {
				ImGui::BeginDisabled(!on);
				if (FamilyUI::AccentButton("Drag icons on screen##tab")) {
					StartDragging();
				}
				ImGui::EndDisabled();
				if (!on) {
					FamilyUI::Wrapped(kOff, "The widget is switched off. Show the widget, on the General page, first.");
				}
			}
			FamilyUI::Footer(on);
		}

		// A page of icons: the Icons page's own, or one an addon asked for.
		void IconsPage(const char* a_title, const char* a_about, std::string_view a_page)
		{
			Top(a_title, a_about);
			StatusTags(false);
			if (FamilyUI::SearchBox(search, kSearchHint)) {
				Found();
			} else {
				ImGui::Spacing();
				DragButton();
				IconSections(a_page);
			}
		}

		void __stdcall RenderIcons()
		{
			FamilyUI::Page page;
			IconsPage("Icons", "Each icon's own switch, size, number and place.", {});
			SaveBar(RestoreEveryIcon, "Restore every icon's defaults");
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
			FamilyUI::Page page;
			IconsPage(pages[N].c_str(), "Each icon's own switch, size, number and place.", pages[N]);
			SaveBar([] { RestoreIconsOn(Extras::Pages()[N]); }, "Restore these icons' defaults");
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

		const auto v = SKSE::PluginDeclaration::GetSingleton()->GetVersion();
		version = std::format("{}.{}.{}", v.major(), v.minor(), v.patch());
		FamilyUI::Use({ FamilyUI::kTeal, "Needs Widget AIO", version.c_str(), FamilyUI::kIconPulse });
		SKSEMenuFramework::SetSection("Needs Widget AIO");
		SKSEMenuFramework::AddSectionItem("General", RenderGeneral);
		SKSEMenuFramework::AddSectionItem("Icons", RenderIcons);
		dragWindow = SKSEMenuFramework::AddWindow(RenderDrag);
		if (dragWindow) {
			SKSEMenuFramework::AddSectionItem("Drag icons", RenderDragTab);
		}
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
