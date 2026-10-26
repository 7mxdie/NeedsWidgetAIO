#include "SettingsUI.h"

#include "Extras.h"
#include "Hotkey.h"
#include "HudLayer.h"
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

		bool saveFailed = false;  // the last Save couldn't write the file

		void Footer(void (*a_restore)(), const char* a_restoreText)
		{
			ImGui::Separator();
			if (ImGui::Button("Save settings")) {
				saveFailed = !Settings::Save();
			}
			if (Settings::HasUnsavedChanges()) {
				ImGui::SameLine();
				// Bright, not greyed out: players took moving the sliders for saving (2.3.0).
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.3f, 1.0f));
				ImGui::Text("%s", saveFailed ? "Could not save. NeedsWidget.log says why." : "Not saved yet. Press Save settings to keep it.");
				ImGui::PopStyleColor();
			}
			ImGui::SameLine();
			if (ImGui::Button(a_restoreText)) {
				a_restore();
			}
			Hint("Save settings saves both pages at once.");
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
			if (dragWindow && ImGui::Button("Drag icons on screen")) {
				StartDragging();
			}
			Hint("Move the icons with the mouse, and drag their corners to resize them. Done brings this menu back.");
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
			if (ImGui::Button("Save settings##drag")) {
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
			if (Settings::HasUnsavedChanges()) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.3f, 1.0f));
				ImGui::Text("%s", saveFailed ? "Could not save. NeedsWidget.log says why." : "Not saved yet. Press Save settings to keep it.");
				ImGui::PopStyleColor();
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
				changed |= ImGui::Checkbox("Hide when fine##menu", &icon.hideWhenFine);
				if (ImGui::Button("Hide this icon##menu")) {
					icon.enabled = false;
					changed = true;
					ImGui::CloseCurrentPopup();
				}
				Hint("Show this icon, in its section on the Icons page, brings it back.");
				if (changed) {
					Settings::Set(id, icon);
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
			auto* draw = ImGui::GetWindowDrawList();
			bool  anyHeld = false;
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
						ImGui::ImDrawListManager::AddLine(draw, { guideX, 0.0f }, { guideX, h }, 0x904DBFFF, 1.0f);
					}
					if (guideY >= 0.0f) {
						ImGui::ImDrawListManager::AddLine(draw, { 0.0f, guideY }, { w, guideY }, 0x904DBFFF, 1.0f);
					}
				}
				anyHeld |= active || sizing;

				// ImGui colours are 0xAABBGGRR: the held one in the same amber as "Not saved yet".
				const bool  busy = active || sizing;
				const ImU32 colour = busy ? 0xFF4DBFFF : hovered || gripHovered ? 0xE0FFFFFF : 0x70FFFFFF;
				ImGui::ImDrawListManager::AddRect(draw, min, max, colour, 4.0f, 0, busy ? 3.0f : 1.5f);
				ImGui::ImDrawListManager::AddRectFilled(draw, gripMin, gripMax, sizing || gripHovered ? 0xFF4DBFFF : colour, 2.0f, 0);
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
			changed |= ImGui::SliderFloat("Distance from the icon", &g.numberGap, -60.0f, 60.0f, "%.0f");
			changed |= ImGui::SliderFloat("Move sideways", &g.numberX, -200.0f, 200.0f, "%.0f");
			changed |= ImGui::RadioButton("Centred", &g.numberAlign, 0);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("Start at the icon##align", &g.numberAlign, 1);
			ImGui::SameLine();
			changed |= ImGui::RadioButton("End at the icon##align", &g.numberAlign, 2);
			Hint("For a column: pick Start at the icon, move the text right with Move sideways and\nup beside the icon with Distance from the icon. The names then line up on their left edge.");
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
			DragButton();
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

		// Its own tab too: Tom looked for it in the tabs first.
		void __stdcall RenderDragTab()
		{
			Settings::MarkEditing();
			ImGui::Text("%s", "Move and resize the icons with the mouse, right on the screen.");
			ImGui::TextDisabled("%s", "This menu closes while you do. Done brings it back.");
			if (dragWindow && ImGui::Button("Drag icons on screen##tab")) {
				StartDragging();
			}
		}

		void __stdcall RenderIcons()
		{
			DragButton();
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
			DragButton();
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
