#pragma once

#include "Needs.h"
#include "Settings.h"

// The widget's own always-open menu, drawn over the HUD the same way STB Widgets
// and TrueHUD draw theirs. It loads Interface/NeedsWidget/widget.swf and pushes
// the look, the layout and the values into it from the game's UI thread.
class HudLayer : public RE::IMenu
{
public:
	static constexpr std::string_view MENU_NAME = "NeedsWidget";
	static constexpr std::string_view FILE_NAME = "NeedsWidget/widget";

	HudLayer();
	~HudLayer() override;

	static void Register();
	static void Show();

	void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

	// Where an icon showing is: its centre and half its size, as shares of the screen.
	struct Spot
	{
		std::uint32_t id{ 0 };
		float         x{ 0.0f };
		float         y{ 0.0f };
		float         halfW{ 0.0f };
		float         halfH{ 0.0f };
	};

	// While the settings' drag screen is open the widget is asked every frame where
	// its icons are (the UI thread), and the drag screen reads it (the render thread).
	static void              WatchSpots(bool a_on) { watchSpots = a_on; }
	static std::vector<Spot> Spots();

	// SKSE says when a save starts and finishes loading; no values are read in between.
	static void SetGameLoading(bool a_on) { gameLoading = a_on; }

private:
	// What the widget was last told about one icon, so only changes are sent.
	struct Sent
	{
		bool        visible{ false };
		float       fraction{ -1.0f };
		int         stage{ -1 };
		std::string label;

		bool operator==(const Sent&) const = default;
	};

	static RE::IMenu* Create() { return new HudLayer(); }
	static inline std::atomic_bool gameLoading{ false };  // between SKSE's kPreLoadGame and kPostLoadGame

	std::string_view HiddenBy() const;  // empty when the widget should show, else why not
	bool SendStyle();
	bool SendLayout(Needs::Id a_id);
	std::pair<float, float> LineOffset(Needs::Id a_id, const Settings::General& a_general) const;  // along and across the row or column, movie units
	bool Call(const char* a_function, std::initializer_list<RE::GFxValue> a_args);

	bool                            shown{ false };
	bool                            stylePending{ true };
	std::array<bool, Needs::kCount> artPending{};  // an extra icon's picture, named by its addon file, not sent yet
	std::array<bool, Needs::kCount> layoutPending{};
	std::array<Sent, Needs::kCount> sent{};
	std::array<bool, Needs::kCount> preview{};   // shown faded only because a settings page is open
	std::array<bool, Needs::kCount> onScreen{};  // what the widget was last told is visible, for closing gaps
	float                           sinceValueUpdate{ 0.0f };
	std::array<int, Needs::kCount>  liveStage{};         // last stage while the icon was live, -1 when it wasn't
	float                           sinceChange{ 0.0f };  // seconds since any icon came, went or changed stage
	bool                            keyHidden{ false };   // hidden by the hotkey
	float                           fadeTarget{ -1.0f };  // what the widget was last told to fade to

	void UpdateSpots();

	static inline std::atomic<bool> watchSpots{ false };
	static inline std::mutex        spotLock;
	static inline std::vector<Spot> spots;
};
