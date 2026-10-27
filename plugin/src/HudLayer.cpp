#include "HudLayer.h"

#include "Extras.h"
#include "Hotkey.h"
#include "Settings.h"

#include <cstdarg>
#include <cstdio>

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
	for (std::uint32_t id = Needs::kBuiltInCount; id < Needs::kCount; ++id) {
		artPending[id] = Extras::Get(static_cast<Needs::Id>(id)) != nullptr;
	}

	auto* scaleform = RE::BSScaleformManager::GetSingleton();
	const bool loaded = scaleform->LoadMovieEx(this, FILE_NAME, [](RE::GFxMovieDef* a_def) {
		a_def->SetState(RE::GFxState::StateType::kLog, RE::make_gptr<WidgetLog>().get());
	});
	logs::info("widget movie {}", loaded && uiMovie ? "loaded" : "FAILED to load");
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
	auto* ui = RE::UI::GetSingleton();
	if (!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME)) {
		return "the HUD is not open"sv;
	}
	for (const auto& name : HIDE_IN) {
		if (ui->IsMenuOpen(name)) {
			return name;
		}
	}
	return {};
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
	} else if (general.look == Settings::Look::kIcon) {
		look = "icon";
		frame.clear();
		artSize = s.iconArtSize;
		artY = s.iconArtY;
		labelY = s.iconLabelY;
	}
	// The label's top sits at labelY. Its digits run from about 3 to 15 units down
	// the field at the 16-unit base size (2 of gutter, then the ascent), so above
	// the icon, mirroring the digits themselves keeps the same gap as below.
	const float k = general.numberSize / 100.0f;
	labelY += general.numberGap;
	if (general.numbersAbove) {
		labelY = -labelY - (DIGITS_BOTTOM + DIGITS_TOP) * k;
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
	std::vector<Needs::Id> line;
	for (const auto id : a_general.order) {
		const auto need = static_cast<Needs::Id>(id);
		if (Needs::Installed(need) && (!a_general.closeGaps || onScreen[id])) {
			line.push_back(static_cast<Needs::Id>(id));
		}
	}
	if (a_general.fromFarEnd) {
		std::reverse(line.begin(), line.end());
	}
	const float direction = a_general.fromFarEnd ? -1.0f : 1.0f;
	const float k = a_general.groupSize / 100.0f;
	const auto  perLine = a_general.perLine > 0 ? static_cast<std::size_t>(a_general.perLine) : line.size();
	float       widest = 0.0f;
	for (const auto id : line) {
		widest = (std::max)(widest, ICON_UNITS * Settings::Get(id).size / 100.0f * k);
	}
	const float across = (a_general.wrapBefore ? -1.0f : 1.0f) * widest * (1.0f + a_general.lineGap / 100.0f);
	float       at = 0.0f;
	float       lastWidth = 0.0f;
	for (std::size_t n = 0; n < line.size(); ++n) {
		const float width = ICON_UNITS * Settings::Get(line[n]).size / 100.0f * k;
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
	float x = g.lineX;
	float y = g.lineY;
	float size = i.size * g.groupSize / 100.0f;
	float dx = 0.0f;
	float dy = 0.0f;
	if (g.arrangement == Settings::Arrangement::kFree) {
		const auto p = Settings::Place(i, g);
		x = p.x;
		y = p.y;
		size = p.size;
	} else if (g.arrangement == Settings::Arrangement::kRow) {
		std::tie(dx, dy) = LineOffset(a_id, g);
	} else {
		std::tie(dy, dx) = LineOffset(a_id, g);
	}
	const float opacity = preview[a_id] ? i.opacity * PREVIEW_OPACITY : i.opacity;
	return Call("_root.setLayout", { RE::GFxValue(static_cast<double>(a_id)), RE::GFxValue(x), RE::GFxValue(y),
		RE::GFxValue(size), RE::GFxValue(opacity), RE::GFxValue(i.showNumber), RE::GFxValue(dx), RE::GFxValue(dy) });
}

void HudLayer::AdvanceMovie(float a_interval, std::uint32_t a_currentTime)
{
	// Advance first: the widget's functions only exist once frame 1 has run, and an
	// Invoke before that fails silently. Every update below is retried until the
	// widget actually accepts it.
	RE::IMenu::AdvanceMovie(a_interval, a_currentTime);

	if (Settings::ConsumeDirty()) {
		stylePending = true;
		layoutPending.fill(true);
		sinceValueUpdate = VALUE_INTERVAL;  // so a ticked box shows at once
	}
	if (stylePending && SendStyle()) {
		stylePending = false;
	}
	// The widget knows the built-in needs' pictures; an extra icon's comes from its file.
	for (std::uint32_t id = Needs::kBuiltInCount; id < Needs::kCount; ++id) {
		const auto* extra = Extras::Get(static_cast<Needs::Id>(id));
		if (artPending[id] && extra && Call("_root.setArt", { RE::GFxValue(static_cast<double>(id)), RE::GFxValue(extra->art.c_str()) })) {
			artPending[id] = false;
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
		if (g.hotkeyMode != 0) {
			keyHidden = false;
		}
		if (const int presses = Hotkey::TakePresses(); presses > 0) {
			if (g.hotkeyMode == 0) {
				keyHidden = presses % 2 == 1 ? !keyHidden : keyHidden;
				logs::info("hotkey: widget {}", keyHidden ? "hidden" : "shown");
			} else {
				sinceChange = 0.0f;
			}
		}
		if (editing) {
			sinceChange = 0.0f;
		}
		const bool anyBad = std::ranges::any_of(liveStage, [](int a_stage) { return a_stage >= BAD_STAGE; });
		float      target = 100.0f;
		if (!editing && keyHidden) {
			target = 0.0f;
		} else if (!editing && g.fadeWhenStill && sinceChange > g.fadeAfter && !(g.keepBad && anyBad)) {
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
	sinceValueUpdate = 0.0f;
	const auto numberShows = Settings::GetGeneral().numberShows;
	const auto numberCase = Settings::GetGeneral().numberCase;
	const auto meterDrains = Settings::GetGeneral().meterDrains;
	const auto flashWorse = Settings::GetGeneral().flashWorse;
	std::array<Sent, Needs::kCount> next{};
	for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
		const auto need = static_cast<Needs::Id>(id);
		if (!Needs::Used(need)) {
			continue;  // an empty extra slot: never shown
		}
		const auto r = Needs::Read(need);
		const auto icon = Settings::Get(need);
		Sent s;
		const bool live = icon.enabled && r.available && !(icon.hideWhenFine && r.stage <= Needs::FineUpTo(need));
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
		} else if (numberShows == 1 && r.stageName) {
			s.label = r.stageName;
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

		// Anything that comes, goes or changes stage brings a faded widget back; a
		// stage that gets worse flashes its icon.
		const int now = live ? r.stage : -1;
		if (now != liveStage[id]) {
			if (live && liveStage[id] >= 0 && now > liveStage[id] && flashWorse) {
				Call("_root.flash", { RE::GFxValue(static_cast<double>(id)) });
			}
			liveStage[id] = now;
			sinceChange = 0.0f;
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
	if (lineChanged && general.closeGaps && general.arrangement != Settings::Arrangement::kFree) {
		for (std::uint32_t id = 0; id < Needs::kCount; ++id) {
			layoutPending[id] = !SendLayout(static_cast<Needs::Id>(id));
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
