#include "HudLayer.h"

#include "Styles.h"

#include "Extras.h"
#include "Hotkey.h"
#include "Settings.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>

// Windows.h makes PlaySound a macro for its own PlaySoundW; the game's RE::PlaySound is the one meant here.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace
{
	// Menus that cover the HUD. While any of these is open the widget hides, the
	// same as the vanilla HUD does. SKSE Menu Framework is not a game menu, so the
	// widget stays up while its sliders are being dragged.
	constexpr std::array HIDE_IN = {
		"Loading Menu"sv, "Main Menu"sv, "Dialogue Menu"sv, "Journal Menu"sv, "InventoryMenu"sv,
		"MagicMenu"sv, "MapMenu"sv, "StatsMenu"sv, "TweenMenu"sv, "Console"sv, "ContainerMenu"sv,
		"BarterMenu"sv, "GiftMenu"sv, "Crafting Menu"sv, "Lockpicking Menu"sv, "Book Menu"sv,
		"Sleep/Wait Menu"sv, "RaceSex Menu"sv, "Training Menu"sv, "Creation Club Menu"sv,
		"Mod Manager Menu"sv, "Credits Menu"sv
	};

	// 3.0 "Show in menus" (Fable, Tom): where players decide how long to sleep or what to eat.
	constexpr std::array DECISION_MENUS = { "Sleep/Wait Menu"sv, "InventoryMenu"sv, "BarterMenu"sv, "ContainerMenu"sv, "GiftMenu"sv };
	// Vanilla UI sounds for "play a sound", by General::sound (editor IDs the game looks up itself).
	constexpr std::array SOUNDS = { "UIMenuFocus", "UIObjectiveNew", "UIQuestNew" };
	constexpr float TREND_SECONDS = 8.0f;   // how long the arrow stays after a need moves
	constexpr float TREND_STEP = 0.002f;    // smaller moves are noise
	constexpr float HISTORY_SECONDS = 60.0f;
	constexpr float CALLOUT_MS = 2500.0f;

	// What a stage does to you: the descriptions of the player's active effects whose spell or effect
	// carries the stage's name ("Peckish"), <mag> filled in. Empty when no effect matches (3.0 peek).
	std::string PenaltyText(const char* a_stageName)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* effects = player && a_stageName && *a_stageName ? player->AsMagicTarget()->GetActiveEffectList() : nullptr;
		if (!effects) {
			return {};
		}
		const auto same = [&](const char* a_name) { return a_name && _stricmp(a_name, a_stageName) == 0; };
		std::string out;
		for (auto* ae : *effects) {
			if (!ae || !ae->effect || !ae->effect->baseEffect || ae->flags.any(RE::ActiveEffect::Flag::kInactive)) {
				continue;
			}
			auto* base = ae->effect->baseEffect;
			if (!same(base->GetFullName()) && !(ae->spell && same(ae->spell->GetFullName()))) {
				continue;
			}
			std::string text = base->magicItemDescription.c_str();
			for (const auto* tag : { "<mag>", "<Mag>", "<MAG>" }) {
				for (auto at = text.find(tag); at != std::string::npos; at = text.find(tag)) {
					text.replace(at, 5, std::format("{:.0f}", std::abs(ae->magnitude)));
				}
			}
			if (!text.empty() && out.find(text) == std::string::npos) {
				out += out.empty() ? text : " " + text;
			}
		}
		return out.size() > 80 ? out.substr(0, 77) + "..." : out;
	}

	constexpr float VALUE_INTERVAL = 0.25f;  // seconds between value refreshes
	constexpr float DIGITS_TOP = 3.3f;     // where the number's digits start and end inside its text field,
	constexpr float DIGITS_BOTTOM = 14.8f; // from the top (widget.as: 16-unit $EverywhereMediumFont, 2-unit gutter)
	constexpr float PREVIEW_OPACITY = 0.45f; // a hidden icon shown while a settings page is open
	constexpr float ICON_UNITS = 64.0f;      // an icon's width in the widget's movie units at 100%
	constexpr double FADE_IN_MS = 250.0;
	constexpr double FADE_OUT_MS = 1000.0;
	constexpr int    BAD_STAGE = 4;          // "Stay on screen" counts stages from here up
	// The numbers' fonts, by General::numberFont. The game's own names from fontconfig.txt,
	// so whatever a font mod maps them to is what shows.
	constexpr std::array NUMBER_FONTS = { "$EverywhereMediumFont", "$EverywhereBoldFont", "$EverywhereFont",
		"$ConsoleFont", "$SkyrimBooks", "$HandwrittenFont" };

	// A loading screen (a door, fast travel) can close menus like ours, so the
	// widget disappeared after Tom left the starting room in TEST4. STB Widgets
	// and TrueHUD open theirs again the same way: whenever a loading screen
	// closes, or the HUD opens, put ours back if it is gone.
	class MenuWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuWatch* Get()
		{
			static MenuWatch watch;
			return &watch;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const bool loadingClosed = a_event->menuName == RE::LoadingMenu::MENU_NAME && !a_event->opening;
			const bool hudOpened = a_event->menuName == RE::HUDMenu::MENU_NAME && a_event->opening;
			auto* ui = RE::UI::GetSingleton();
			if ((loadingClosed || hudOpened) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && !ui->IsMenuOpen(HudLayer::MENU_NAME)) {
				logs::info("widget menu was closed; opening it again after {}", loadingClosed ? "the loading screen" : "the HUD opened");
				HudLayer::Show();
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// Scaleform's log for this movie, the widget's own trace() lines included,
	// goes into NeedsWidget.log. That is how the widget reports which path its
	// pictures loaded from.
	class WidgetLog : public RE::GFxLog
	{
	public:
		void LogMessageVarg(LogMessageType, const char* a_fmt, std::va_list a_args) override
		{
			std::array<char, 1024> buf{};
			std::vsnprintf(buf.data(), buf.size(), a_fmt ? a_fmt : "", a_args);
			std::string line(buf.data());
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
				line.pop_back();
			}
			if (!line.empty()) {
				logs::info("[widget] {}", line);
			}
		}
	};
}

HudLayer::HudLayer()
{
	using Flag = RE::UI_MENU_FLAGS;
	menuFlags.set(Flag::kAlwaysOpen, Flag::kRequiresUpdate, Flag::kAllowSaving);
	depthPriority = 0;
	inputContext = Context::kNone;
	layoutPending.fill(true);
	liveStage.fill(-1);
	stageNow.fill(-1);
	trendFrom.fill(-1.0f);
	// What widget.as starts with: the built-in needs' own pictures, nothing in the extra slots.
	for (std::uint32_t id = 0; id < Needs::kBuiltInCount; ++id) {
		sentArt[id] = Needs::DefaultArt(static_cast<Needs::Id>(id));
	}

	auto* scaleform = RE::BSScaleformManager::GetSingleton();
	// A movie's log only starts once it has loaded, so Scaleform's reason for refusing one
	// goes nowhere. Lend the loader ours for the load (lolallamaka: file there, right size, refused).
	auto* gameLog = scaleform->loader ? scaleform->loader->GetStateAddRef(RE::GFxState::StateType::kLog) : nullptr;
	if (scaleform->loader) {
		scaleform->loader->SetState(RE::GFxState::StateType::kLog, RE::make_gptr<WidgetLog>().get());
	}
	bool loaded = scaleform->LoadMovieEx(this, FILE_NAME, [](RE::GFxMovieDef* a_def) {
		a_def->SetState(RE::GFxState::StateType::kLog, RE::make_gptr<WidgetLog>().get());
	});
	if (!loaded || !uiMovie) {
		// CommonLib's copy of the load failed; the game's own is what every vanilla menu uses.
		loaded = scaleform->LoadMovie(this, uiMovie, FILE_NAME.data(), RE::BSScaleformManager::ScaleModeType::kShowAll, 0.0f);
		logs::info("widget movie: the game's own load {}", loaded && uiMovie ? "worked" : "failed too");
	}
	if (!loaded || !uiMovie) {
		// Both of those stop at the game's FileExists, and on lolallamaka's game it says no to a
		// file that's there. Skip the check and hand Scaleform the file itself.
		for (const auto* name : { "Interface/HUDMenu.swf", "Interface/NeedsWidget/icons/hunger.swf", "Interface/NeedsWidget/widget.swf", "Interface\\NeedsWidget\\widget.swf", "interface/needswidget/widget.swf", "Data/Interface/NeedsWidget/widget.swf" }) {
			logs::info("the game sees {}: {}", name, scaleform->FileExists(name));
		}
		std::error_code ec;
		const auto      full = std::filesystem::absolute("Data/Interface/NeedsWidget/widget.swf", ec).string();
		for (const auto& path : { std::string("Interface/NeedsWidget/widget.swf"), std::string("Data/Interface/NeedsWidget/widget.swf"), full }) {
			loaded = LoadDirect(path);
			logs::info("widget movie: straight from {}: {}", path, loaded ? "worked" : "failed");
			if (loaded) {
				break;
			}
		}
	}
	if (scaleform->loader) {
		scaleform->loader->SetState(RE::GFxState::StateType::kLog, gameLog);
	}
	if (gameLog) {
		gameLog->Release();
	}
	logs::info("widget movie {}", loaded && uiMovie ? "loaded" : "FAILED to load");
	if (!loaded || !uiMovie) {
		// Two reports (swk09, lolallamaka) with nothing to tell a missing file from one the
		// game couldn't read. A file packed in a BSA isn't seen here; ours ships loose.
		std::error_code             ec;
		const std::filesystem::path swf = "Data/Interface/NeedsWidget/widget.swf";
		const auto                  size = std::filesystem::file_size(swf, ec);
		if (ec) {
			logs::warn("{} is not there, so the icons can't show. Reinstall the mod with its Interface folder.", swf.string());
		} else {
			// Right size doesn't prove right bytes: the header says how long the file should be.
			std::array<char, 8> head{};
			std::ifstream       in(swf, std::ios::binary);
			in.read(head.data(), head.size());
			std::uint32_t declared = 0;
			std::memcpy(&declared, head.data() + 4, sizeof(declared));
			logs::warn("{} is there, {} bytes, but the game couldn't load it. It opened here: {}; starts {}{}{}, says it is {} bytes; the game sees it: {}", swf.string(), size,
				in.good(), head[0], head[1], head[2], declared, scaleform->FileExists("Interface/NeedsWidget/widget.swf"));
		}
	}
}

// BSScaleformManager::LoadMovieEx without its check that the game can see the file.
bool HudLayer::LoadDirect(const std::string& a_path)
{
	using LoadConstants = RE::GFxLoader::LoadConstants;
	auto* loader = RE::BSScaleformManager::GetSingleton()->loader;
	auto* def = loader ? loader->CreateMovie(a_path.c_str(), static_cast<LoadConstants>(LoadConstants::kLoadKeepBindData | LoadConstants::kLoadWaitFrame1)) : nullptr;
	if (!def) {
		return false;
	}
	def->SetState(RE::GFxState::StateType::kLog, RE::make_gptr<WidgetLog>().get());
	uiMovie.reset(def->CreateInstance());
	if (!uiMovie) {
		return false;
	}
	uiMovie->Release();
	uiMovie->SetViewScaleMode(RE::BSScaleformManager::ScaleModeType::kShowAll);
	uiMovie->SetBackgroundAlpha(0.0f);
	if (const auto* state = RE::BSGraphics::State::GetSingleton()) {
		RE::GViewport view;
		view.bufferWidth = view.width = state->screenWidth;
		view.bufferHeight = view.height = state->screenHeight;
		uiMovie->SetViewport(view);
	}
	uiMovie->Advance(0.0f);
	RefreshPlatform();
	return true;
}

HudLayer::~HudLayer()
{
	logs::info("widget menu closed");
}

void HudLayer::Register()
{
	auto* ui = RE::UI::GetSingleton();
	ui->Register(MENU_NAME, Create);
	ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatch::Get());
	logs::info("menu registered");
}

void HudLayer::Show()
{
	RE::UIMessageQueue::GetSingleton()->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
}

std::string_view HudLayer::HiddenBy() const
{
	auto*      ui = RE::UI::GetSingleton();
	const auto g = Settings::GetGeneral();
	if (!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
		return "the HUD is not open"sv;
	}
	const bool decision = g.showInMenus && InDecisionMenu();
	for (const auto& name : HIDE_IN) {
		if (ui->IsMenuOpen(name) && !(decision && std::ranges::find(DECISION_MENUS, name) != DECISION_MENUS.end())) {
			return name;
		}
	}
	if (!g.showWidget) {
		return "switched off on the General page"sv;
	}
	// 3.0 situations (Fable, Tom). Holding the peek key shows it anyway; so does a settings page.
	if (peeking || Settings::IsEditing()) {
		return {};
	}
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		if (g.hideInCombat && player->IsInCombat()) {
			return "in combat (Hide in combat)"sv;
		}
		if (g.hideWeaponDrawn && player->AsActorState()->IsWeaponDrawn()) {
			return "weapon drawn (Hide with a weapon out)"sv;
		}
		if (g.hideMounted && player->IsOnMount()) {
			return "riding (Hide while riding)"sv;
		}
	}
	return {};
}

bool HudLayer::InDecisionMenu() const
{
	auto* ui = RE::UI::GetSingleton();
	return std::ranges::any_of(DECISION_MENUS, [&](std::string_view a_name) { return ui->IsMenuOpen(a_name); });
}

float HudLayer::IconScale(Needs::Id a_id, const Settings::General& a_general) const
{
	return a_general.growBad > 0.0f && stageNow[a_id] >= BAD_STAGE ? 1.0f + a_general.growBad / 100.0f : 1.0f;
}

// The icons in a row or column, in their place order: the player's, or worst first (3.0), and
// only one group's. Icons left out (not installed, off screen with Close gaps) are not in it.
std::vector<Needs::Id> HudLayer::Line(int a_group, const Settings::General& a_general) const
{
	std::vector<Needs::Id> line;
	for (const auto id : a_general.order) {
		const auto need = static_cast<Needs::Id>(id);
		if (Needs::Installed(need) && (!a_general.closeGaps || onScreen[id]) && Settings::Get(need).group == a_group) {
			line.push_back(need);
		}
	}
	if (a_general.worstFirst) {
		std::ranges::stable_sort(line, [&](Needs::Id a, Needs::Id b) { return stageNow[a] > stageNow[b]; });
	}
	return line;
}

bool HudLayer::Call(const char* a_function, std::initializer_list<RE::GFxValue> a_args)
{
	return uiMovie && uiMovie->Invoke(a_function, nullptr, a_args.begin(), static_cast<std::uint32_t>(a_args.size()));
}

std::vector<HudLayer::Spot> HudLayer::Spots()
{
	std::scoped_lock l(spotLock);
	return spots;
}

void HudLayer::UpdateSpots()
{
	std::vector<Spot> found;
	RE::GFxValue      answer;
	if (watchSpots && uiMovie && uiMovie->Invoke("_root.spots", &answer, nullptr, 0) && answer.IsString()) {
		// "id,x,y,halfW,halfH;" for each icon showing (widget.as, spots).
		std::istringstream in(answer.GetString());
		std::string        item;
		while (std::getline(in, item, ';')) {
			Spot s;
			if (sscanf_s(item.c_str(), "%u,%f,%f,%f,%f", &s.id, &s.x, &s.y, &s.halfW, &s.halfH) == 5 && s.id < Needs::kCount) {
				found.push_back(s);
			}
		}
	}
	std::scoped_lock l(spotLock);
	spots = std::move(found);
}

bool HudLayer::SendStyle()
{
	const auto general = Settings::GetGeneral();
	const auto s = Settings::GetStyle();
	// 3.0 icon styles: the style's own meter goes first (setShape), then setStyle's look "shape".
	const auto picked = Styles::Find(general.style);
	if (picked) {
		const auto& st = *picked;
		if (!Call("_root.setShape", { RE::GFxValue(st.meter.c_str()), RE::GFxValue(st.width), RE::GFxValue(st.geometry.c_str()),
					RE::GFxValue(st.by.c_str()), RE::GFxValue(st.alpha), RE::GFxValue(static_cast<double>(st.glyph)), RE::GFxValue(st.tintFrame) })) {
			return false;
		}
	} else if (!Call("_root.setShape", { RE::GFxValue(""), RE::GFxValue(0.0), RE::GFxValue(""), RE::GFxValue(""), RE::GFxValue(100),
					   RE::GFxValue(-1.0), RE::GFxValue(false) })) {
		return false;
	}
	std::string look = "ring";
	std::string frame = s.ringFrame;
	float       artSize = s.ringArtSize;
	float       artY = s.ringArtY;
	float       labelY = s.ringLabelY;
	if (general.look == Settings::Look::kBar) {
		look = "bar";
		frame = s.barFrame;
		artSize = s.barArtSize;
		artY = s.barArtY;
		labelY = s.barLabelY;
	} else if (general.look == Settings::Look::kFill) {
		look = "fill";  // the ring's frame, sizes and number place; the widget fills inside it
	} else if (general.look == Settings::Look::kIcon) {
		look = "icon";
		frame.clear();
		artSize = s.iconArtSize;
		artY = s.iconArtY;
		labelY = s.iconLabelY;
	}
	// A style's numbers go just past its own bottom (or top, above), the same gap as the Ring's.
	float aboveY = labelY;
	if (picked) {
		look = "shape";
		frame = picked->frame;
		artSize = picked->artSize;
		artY = picked->artY;
		labelY = picked->bottom + 2.0f;
		aboveY = -picked->top + 2.0f;
	}
	// The label's top sits at labelY. Its digits run from about 3 to 15 units down
	// the field at the 16-unit base size (2 of gutter, then the ascent), so above
	// the icon, mirroring the digits themselves keeps the same gap as below.
	const float k = general.numberSize / 100.0f;
	// Stage pips (3.0, for colour-blind players) sit just under the icon; the numbers move down past them.
	const float pipsY = labelY - 1.0f;
	if (general.stagePips) {
		labelY += 6.0f;
	}
	labelY += general.numberGap;
	if (general.numbersAbove) {
		labelY = -(aboveY + general.numberGap) - (DIGITS_BOTTOM + DIGITS_TOP) * k;
	}
	const auto c = Settings::StageColors(general);
	return Call("_root.setStyle", {
		RE::GFxValue(look.c_str()), RE::GFxValue(frame.c_str()),
		RE::GFxValue(artSize), RE::GFxValue(artY),
		RE::GFxValue(s.ringRadius), RE::GFxValue(s.ringWidth),
		RE::GFxValue(s.barLeft), RE::GFxValue(s.barTop), RE::GFxValue(s.barLength), RE::GFxValue(s.barHeight),
		RE::GFxValue(labelY), RE::GFxValue(general.tintArt),
		RE::GFxValue(static_cast<double>(c[0])), RE::GFxValue(static_cast<double>(c[1])), RE::GFxValue(static_cast<double>(c[2])),
		RE::GFxValue(static_cast<double>(c[3])), RE::GFxValue(static_cast<double>(c[4])), RE::GFxValue(static_cast<double>(c[5])),
		RE::GFxValue(general.pulseWorst) }) &&
	       Call("_root.setPips", { RE::GFxValue(general.stagePips), RE::GFxValue(pipsY) }) &&
	       Call("_root.setNumbers", { RE::GFxValue(general.numberSize), RE::GFxValue(general.numberTint), RE::GFxValue(general.numberX), RE::GFxValue(general.numberEdge), RE::GFxValue(general.numberAlign),
									RE::GFxValue(NUMBER_FONTS[std::clamp(general.numberFont, 0, 5)]) });
}

// Where an icon's centre sits along its row or column, in movie units, from the
// icon at the chosen end. Each step is half of one icon, the gap, and half of the
// next, so icons of different sizes still sit evenly. An icon left out of the
// line (hidden, with Close gaps on, or its mod not in the game) gets 0; it is not
// shown anyway. With a limit per row or column, the line starts again from the
// same end one row (or column) over, every row as far apart as the biggest icon.
std::pair<float, float> HudLayer::LineOffset(Needs::Id a_id, const Settings::General& a_general) const
{
	auto line = Line(Settings::Get(a_id).group, a_general);
	if (a_general.fromFarEnd) {
		std::reverse(line.begin(), line.end());
	}
	const float direction = a_general.fromFarEnd ? -1.0f : 1.0f;
	const float k = a_general.groupSize / 100.0f;
	const auto  perLine = a_general.perLine > 0 ? static_cast<std::size_t>(a_general.perLine) : line.size();
	float       widest = 0.0f;
	for (const auto id : line) {
		widest = (std::max)(widest, ICON_UNITS * Settings::Get(id).size / 100.0f * k * IconScale(id, a_general));
	}
	const float across = (a_general.wrapBefore ? -1.0f : 1.0f) * widest * (1.0f + a_general.lineGap / 100.0f);
	float       at = 0.0f;
	float       lastWidth = 0.0f;
	for (std::size_t n = 0; n < line.size(); ++n) {
		const float width = ICON_UNITS * Settings::Get(line[n]).size / 100.0f * k * IconScale(line[n], a_general);
		if (n % perLine == 0) {
			at = 0.0f;
		} else {
			const float gap = a_general.spacing / 100.0f * (lastWidth + width) / 2.0f;
			at += direction * (lastWidth / 2.0f + gap + width / 2.0f);
		}
		if (line[n] == a_id) {
			return { at, static_cast<float>(n / perLine) * across };
		}
		lastWidth = width;
	}
	return { 0.0f, 0.0f };
}

bool HudLayer::SendLayout(Needs::Id a_id)
{
	const auto i = Settings::Get(a_id);
	const auto g = Settings::GetGeneral();
	const bool second = i.group == 1 && g.arrangement != Settings::Arrangement::kFree;
	float      x = second ? g.line2X : g.lineX;
	float      y = second ? g.line2Y : g.lineY;
	float      size = i.size * g.groupSize / 100.0f * IconScale(a_id, g);
	float dx = 0.0f;
	float dy = 0.0f;
	if (g.arrangement == Settings::Arrangement::kFree) {
		const auto p = Settings::Place(i, g);
		x = p.x;
		y = p.y;
		size = p.size * IconScale(a_id, g);
	} else if (g.arrangement == Settings::Arrangement::kRow) {
		std::tie(dx, dy) = LineOffset(a_id, g);
	} else {
		std::tie(dy, dx) = LineOffset(a_id, g);
	}
	const float opacity = preview[a_id] ? i.opacity * PREVIEW_OPACITY : i.opacity;
	return Call("_root.setLayout", { RE::GFxValue(static_cast<double>(a_id)), RE::GFxValue(x), RE::GFxValue(y),
		RE::GFxValue(size), RE::GFxValue(opacity), RE::GFxValue(i.showNumber), RE::GFxValue(dx), RE::GFxValue(dy),
		RE::GFxValue(i.numberX), RE::GFxValue(i.numberY) });
}

void HudLayer::AdvanceMovie(float a_interval, std::uint32_t a_currentTime)
{
	// Advance first: the widget's functions only exist once frame 1 has run, and an
	// Invoke before that fails silently. Every update below is retried until the
	// widget actually accepts it.
	RE::IMenu::AdvanceMovie(a_interval, a_currentTime);

	clock += a_interval;
	{
		// Peek (3.0): the key held in mode 2, or a decision menu with "show in menus".
		const auto g = Settings::GetGeneral();
		const bool key = g.hotkeyMode == 2 && Hotkey::Held();
		const bool peek = key || (g.showInMenus && InDecisionMenu());
		if (peek != peeking || key != peekKey) {
			peeking = peek;
			peekKey = key;
			layoutPending.fill(true);
			sinceValueUpdate = VALUE_INTERVAL;
			sinceChange = 0.0f;
			logs::info("peek {}", peek ? "on" : "off");
		}
	}
	if (Settings::ConsumeDirty()) {
		stylePending = true;
		layoutPending.fill(true);
		sinceValueUpdate = VALUE_INTERVAL;  // so a ticked box shows at once
	}
	if (stylePending && SendStyle()) {
		stylePending = false;
	}
	// Each icon's picture: an extra icon's comes from its file, and since 3.0 the player can pick
	// any picture in icons/ for any icon (sArt). Sent whenever it differs from what's showing.
	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		const auto need = static_cast<Needs::Id>(id);
		if (!Needs::Used(need)) {
			continue;
		}
		const auto art = Needs::Art(need);
		if (!art.empty() && art != sentArt[id] && Call("_root.setArt", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(art.c_str()) })) {
			sentArt[id] = art;
		}
	}
	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		if (layoutPending[id] && SendLayout(static_cast<Needs::Id>(id))) {
			layoutPending[id] = false;
		}
	}

	const auto hiddenBy = HiddenBy();
	const bool want = hiddenBy.empty();
	if (want != shown && Call("_root.setShown", { RE::GFxValue(want) })) {
		shown = want;
		// Every show and hide goes in the log, with the reason, so a widget that goes
		// missing says why (TEST4: it vanished after the first door).
		if (want) {
			logs::info("widget shown");
		} else {
			logs::info("widget hidden: {}", hiddenBy);
		}
	}

	// Fading and the hotkey (TEST10). A settings page open keeps everything up.
	const bool editing = Settings::IsEditing();
	{
		const auto g = Settings::GetGeneral();
		sinceChange += a_interval;
		if (const int presses = Hotkey::TakePresses(); presses > 0) {
			if (g.hotkeyMode == 2) {
				// Hold to peek: handled above, from the key's held state.
			} else if (g.hotkeyMode == 0) {
				// The same switch as Show the widget on the General page, kept at once, so
				// there is one "off" and it lasts past quitting (2.10).
				if (presses % 2 == 1) {
					Settings::SaveShowWidget(!g.showWidget);
					logs::info("hotkey: widget {}", g.showWidget ? "hidden" : "shown");
				}
			} else {
				sinceChange = 0.0f;
			}
		}
		if (editing || peeking) {
			sinceChange = 0.0f;
		}
		const bool anyBad = std::ranges::any_of(liveStage, [](int a_stage) { return a_stage >= BAD_STAGE; });
		float      target = 100.0f;
		if (!editing && g.fadeWhenStill && sinceChange > g.fadeAfter && !(g.keepBad && anyBad)) {
			target = 0.0f;
		}
		if (target != fadeTarget && Call("_root.setFade", { RE::GFxValue(target), RE::GFxValue(target > fadeTarget ? FADE_IN_MS : FADE_OUT_MS) })) {
			if (fadeTarget >= 0.0f) {
				logs::info("widget faded {}", target > 0.0f ? "in" : "out");
			}
			fadeTarget = target;
		}
	}
	UpdateSpots();

	sinceValueUpdate += a_interval;
	if (sinceValueUpdate < VALUE_INTERVAL) {
		return;
	}
	// While a save is loading the game is rebuilding the player and the scripts, and this menu
	// can still be ticking: reading them now could touch what's being replaced. Keep the last
	// values until it's done (McRaimix91, crash after loading; not proven to be this).
	auto* ui = RE::UI::GetSingleton();
	if (gameLoading || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
		return;
	}
	sinceValueUpdate = 0.0f;
	const auto numberShows = Settings::GetGeneral().numberShows;
	const auto numberCase = Settings::GetGeneral().numberCase;
	const auto meterDrains = Settings::GetGeneral().meterDrains;
	const auto flashWorse = Settings::GetGeneral().flashWorse;
	const auto general0 = Settings::GetGeneral();
	auto*      player = RE::PlayerCharacter::GetSingleton();
	auto*      cell = player ? player->GetParentCell() : nullptr;
	const bool indoors = cell && cell->IsInteriorCell();
	auto*      calendar = RE::Calendar::GetSingleton();
	const double hours = calendar ? calendar->GetCurrentGameTime() * 24.0 : 0.0;
	bool       orderChanged = false;
	std::array<Sent, Needs::kCount> next{};
	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		const auto need = static_cast<Needs::Id>(id);
		if (!Needs::Used(need)) {
			continue;  // an empty extra slot: never shown
		}
		const auto r = Needs::Read(need);
		const auto icon = Settings::Get(need);
		Sent s;
		const auto hideFrom = Needs::HideFrom(need);
		const bool where = icon.where == 0 || (icon.where == 1 && !indoors) || (icon.where == 2 && indoors);
		bool       live = icon.enabled && r.available && !(icon.hideWhenFine && r.stage <= Needs::FineUpTo(need)) &&
		            !(icon.hideFromStage && hideFrom >= 0 && r.stage >= hideFrom) && where;
		if (peeking && icon.enabled && r.available) {
			live = true;  // peek shows every icon that has a reading
		}
		const int stage = r.available ? r.stage : -1;
		if (stage != stageNow[id]) {
			orderChanged |= general0.worstFirst || (general0.growBad > 0.0f && (stage >= BAD_STAGE) != (stageNow[id] >= BAD_STAGE));
			stageNow[id] = stage;
		}
		// The last minute of readings, for the trend arrow and the time to the worst stage.
		auto& h = history[id];
		if (r.available) {
			h.push_back({ clock, hours, std::clamp(r.fraction, 0.0f, 1.0f) });
		} else {
			h.clear();
		}
		while (!h.empty() && clock - h.front().real > HISTORY_SECONDS) {
			h.pop_front();
		}
		// While one of our settings pages is open, an icon that is switched on but
		// hidden (its mod off, or fine with "Hide when fine") shows faded, so it can
		// still be placed (Tom, TEST3: hunger, cold and fatigue could not be seen
		// with Survival Mode off). Not when its mod is missing: there is nothing to place.
		const bool faded = editing && icon.enabled && !live && Needs::Installed(need);
		if (faded != preview[id]) {
			preview[id] = faded;
			layoutPending[id] = true;
		}
		s.visible = live || faded;
		const float meter = meterDrains ? 1.0f - std::clamp(r.fraction, 0.0f, 1.0f) : std::clamp(r.fraction, 0.0f, 1.0f);
		s.fraction = r.available ? std::round(meter * 500.0f) / 500.0f : 0.0f;
		s.stage = r.available ? r.stage : 0;
		if (!r.available) {
			s.label = "-";
		} else if (numberShows == 2) {
			// Game hours until the worst stage, from how fast it moved over the last minute.
			s.label = "-";
			if (h.size() > 1 && h.back().hours - h.front().hours > 0.05) {
				const double rate = (h.back().f - h.front().f) / (h.back().hours - h.front().hours);
				if (h.back().f >= 0.999f) {
					s.label = "now";
				} else if (rate > 1e-4) {
					const double left = (1.0 - h.back().f) / rate;
					s.label = left >= 1.0 ? std::format("~{:.0f}h", left) : std::format("~{:.0f}m", left * 60.0);
				}
			}
		} else if (numberShows == 1 && r.stageName) {
			s.label = r.stageName;
		} else if (r.showValue) {
			s.label = std::format("{:.0f}", r.value);
		} else {
			s.label = std::format("{:.0f}%", meter * 100.0f);
		}
		if (numberCase != 0) {
			// ASCII only: a stage name in another language keeps its other letters as they are.
			for (auto& c : s.label) {
				if (c >= 'A' && c <= 'Z' && numberCase == 1) {
					c = static_cast<char>(c - 'A' + 'a');
				} else if (c >= 'a' && c <= 'z' && numberCase == 2) {
					c = static_cast<char>(c - 'a' + 'A');
				}
			}
		}
		next[id] = s;
		// Hold to peek (Tom, TEST31: the names under the icons ran into each other): one line per
		// icon in a list beside the widget, "Hunger: Peckish", and what the stage does to you
		// when an active effect carries its name.
		peekLine[id].clear();
		if (peekKey && live && r.available) {
			const std::string stageText = r.stageName ? std::string(r.stageName) : std::format("{:.0f}%", meter * 100.0f);
			const auto        penalty = r.stageName ? PenaltyText(r.stageName) : std::string();
			peekLine[id] = std::format("{}: {}{}|{}", Needs::Name(need), stageText, penalty.empty() ? "" : " - " + penalty, r.stage);
		}

		// Anything that comes, goes or changes stage brings a faded widget back; a
		// stage that gets worse flashes its icon.
		const int now = live ? r.stage : -1;
		if (now != liveStage[id]) {
			const bool known = live && liveStage[id] >= 0;  // not the first reading
			if (known && now > liveStage[id] && flashWorse) {
				Call("_root.flash", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(0) });
			}
			if (known && now < liveStage[id] && general0.flashBetter) {
				Call("_root.flash", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(1) });
			}
			if (known && general0.callout && r.stageName) {
				Call("_root.callout", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(r.stageName), RE::GFxValue(CALLOUT_MS),
										RE::GFxValue(general0.arrangement == Settings::Arrangement::kColumn) });
				logs::info("callout: {} {}", Needs::Name(need), r.stageName);
			}
			if (known && general0.soundFrom > 0 && now >= general0.soundFrom && now > liveStage[id]) {  // every step worse from there (Tom, TEST29)
				RE::PlaySound(SOUNDS[static_cast<std::size_t>(std::clamp(general0.sound, 0, 2))]);
			}
			liveStage[id] = now;
			sinceChange = 0.0f;
		}
		// The trend arrow (Tom, TEST30: it stayed too long): once a need moves, the arrow shows which
		// way for TREND_SECONDS, then goes. A slow need (hunger) moves in steps when its mod's script
		// runs, so each step shows it; a dropped or picked-up item shows it at once.
		int trend = 0;
		if (r.available) {
			const float f = std::clamp(r.fraction, 0.0f, 1.0f);
			if (trendFrom[id] < 0.0f) {
				trendFrom[id] = f;  // the first reading is not a move
			} else if (std::abs(f - trendFrom[id]) > TREND_STEP) {
				trendDir[id] = f > trendFrom[id] ? 1 : -1;
				trendAt[id] = clock;
				trendFrom[id] = f;
			}
		} else {
			trendFrom[id] = -1.0f;
			trendDir[id] = 0;
		}
		if (general0.trend && live && trendDir[id] != 0 && clock - trendAt[id] < TREND_SECONDS) {
			trend = trendDir[id];
		}
		if (trend != trendSent[id] && Call("_root.setTrend", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(trend) })) {
			trendSent[id] = trend;
		}
	}

	// Show at most N (3.0): only the worst N of the icons that would show, ties in the row's order.
	if (general0.maxShown > 0 && !peeking && !editing) {
		std::vector<Needs::Id> showing;
		for (const auto id : general0.order) {
			if (next[id].visible && !preview[id]) {
				showing.push_back(static_cast<Needs::Id>(id));
			}
		}
		std::ranges::stable_sort(showing, [&](Needs::Id a, Needs::Id b) { return stageNow[a] > stageNow[b]; });
		for (std::size_t n = static_cast<std::size_t>(general0.maxShown); n < showing.size(); ++n) {
			next[showing[n]].visible = false;
		}
	}

	// With Close gaps, a row or column closes up when an icon comes or goes. Lay
	// the line out again before telling the widget what is visible, so an icon
	// never shows for a frame in its old place.
	bool lineChanged = false;
	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		if (next[id].visible != onScreen[id]) {
			onScreen[id] = next[id].visible;
			lineChanged = true;
		}
	}
	const auto general = Settings::GetGeneral();
	if (orderChanged) {
		lineChanged = true;  // worst first moved an icon, or one grew or shrank
	}
	if (lineChanged && (general.closeGaps || orderChanged) && (general.arrangement != Settings::Arrangement::kFree || orderChanged)) {
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			layoutPending[id] = !SendLayout(static_cast<Needs::Id>(id));
		}
	}

	// The peek list, in the row's order (first row, then second).
	{
		std::string list;
		if (peekKey) {
			for (const int group : { 0, 1 }) {
				for (const auto id : Line(group, general)) {
					if (!peekLine[id].empty()) {
						list += list.empty() ? peekLine[id] : "\n" + peekLine[id];
					}
				}
			}
		}
		if (list != peekSent && Call("_root.setPeek", { RE::GFxValue(list.c_str()) })) {
			peekSent = list;
		}
	}

	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		const auto& s = next[id];
		if (!(s == sent[id]) && Call("_root.setIcon", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(s.visible),
									RE::GFxValue(static_cast<double>(s.fraction)), RE::GFxValue(static_cast<double>(s.stage)),
									RE::GFxValue(s.label.c_str()) })) {
			sent[id] = s;
		}
	}
}
