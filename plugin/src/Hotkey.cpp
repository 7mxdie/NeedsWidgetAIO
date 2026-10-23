#include "Hotkey.h"

#include "Settings.h"

#include <fstream>

namespace Hotkey
{
	namespace
	{
		constexpr std::uint32_t kEscape = 0x01;

		std::atomic<bool> capturing{ false };
		std::atomic<int>  captured{ 0 };
		std::atomic<bool> refused{ false };
		std::atomic<int>  presses{ 0 };

		// SKSE Menu Framework's own key, from its settings file, so ours can't be the
		// same one. Its names are ImGui's; the common ones are here. Read again
		// whenever the file changes.
		constexpr auto kMenuIni = "Data/SKSE/Plugins/SKSEMenuFramework.ini";

		int ReadMenuKey()
		{
			std::ifstream in(kMenuIni);
			std::string   line;
			std::string   name;
			while (std::getline(in, line)) {
				const auto eq = line.find('=');
				if (line.rfind("ToggleKey", 0) == 0 && line.rfind("ToggleKeyGamePad", 0) != 0 && eq != std::string::npos) {
					name = line.substr(eq + 1);
					std::erase_if(name, [](unsigned char c) { return std::isspace(c); });
					std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
					break;
				}
			}
			static constexpr std::array<int, 10> pad{ 0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49 };
			static constexpr std::array<int, 10> row{ 0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
			static constexpr std::string_view    letters = "QWERTYUIOPASDFGHJKLZXCVBNM";
			static constexpr std::array<int, 26> letterKeys{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
				0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32 };
			static constexpr std::array<std::pair<std::string_view, int>, 18> named{ {
				{ "INSERT", 0xD2 }, { "HOME", 0xC7 }, { "END", 0xCF }, { "DELETE", 0xD3 }, { "PAGEUP", 0xC9 },
				{ "PAGEDOWN", 0xD1 }, { "KP_DECIMAL", 0x53 }, { "KP_DIVIDE", 0xB5 }, { "KP_MULTIPLY", 0x37 },
				{ "KP_SUBTRACT", 0x4A }, { "KP_ADD", 0x4E }, { "KP_ENTER", 0x9C }, { "PAUSE", 0xC5 },
				{ "SCROLLLOCK", 0x46 }, { "NUMLOCK", 0x45 }, { "TAB", 0x0F }, { "GRAVEACCENT", 0x29 }, { "`", 0x29 },
			} };
			int code = 0;
			if (name.size() == 4 && name.rfind("KP_", 0) == 0 && std::isdigit(static_cast<unsigned char>(name[3]))) {
				code = pad[name[3] - '0'];
			} else if (name.size() == 1 && std::isdigit(static_cast<unsigned char>(name[0]))) {
				code = row[name[0] - '0'];
			} else if (name.size() == 1 && letters.find(name[0]) != std::string_view::npos) {
				code = letterKeys[letters.find(name[0])];
			} else if (name.size() >= 2 && name[0] == 'F' && std::isdigit(static_cast<unsigned char>(name[1]))) {
				const int n = std::atoi(name.c_str() + 1);
				code = n >= 1 && n <= 10 ? 0x3A + n : n == 11 ? 0x57 : n == 12 ? 0x58 : 0;
			} else {
				for (const auto& [text, key] : named) {
					if (name == text) {
						code = key;
					}
				}
			}
			logs::info("hotkey: SKSE Menu Framework's key is {} ({})", name.empty() ? "unread" : name, code ? KeyName(code) : "not one we can match");
			return code;
		}

		int MenuKey()
		{
			static std::mutex                      keyLock;
			static int                             key = 0;
			static bool                            read = false;
			static std::filesystem::file_time_type seen{};
			std::scoped_lock                       l(keyLock);
			std::error_code                        ec;
			const auto                             when = std::filesystem::last_write_time(kMenuIni, ec);
			if (ec) {
				return key = 0;
			}
			if (!read || when != seen) {
				read = true;
				seen = when;
				key = ReadMenuKey();
			}
			return key;
		}

		bool Typing()
		{
			auto* controls = RE::ControlMap::GetSingleton();
			return controls && controls->GetRuntimeData().textEntryCount > 0;
		}

		class Keys final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static Keys* GetSingleton()
			{
				static Keys singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				for (auto* e = a_events ? *a_events : nullptr; e; e = e->next) {
					auto* button = e->AsButtonEvent();
					if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) {
						continue;
					}
					const auto key = button->GetIDCode();
					if (capturing) {
						if (key != kEscape && static_cast<int>(key) == MenuKey()) {
							refused = true;
							continue;
						}
						refused = false;
						captured = key == kEscape ? -1 : static_cast<int>(key);
						capturing = false;
						continue;
					}
					const auto wanted = Settings::GetGeneral().hotkey;
					if (wanted <= 0 || static_cast<int>(key) != wanted || wanted == MenuKey()) {
						continue;
					}
					auto* ui = RE::UI::GetSingleton();
					if (!ui || Typing() || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME)) {
						continue;  // only in play
					}
					++presses;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Init()
	{
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink<RE::InputEvent*>(Keys::GetSingleton());
		}
		const auto key = Settings::GetGeneral().hotkey;
		logs::info("hotkey: {}", key > 0 ? KeyName(key) : "none");
		if (key > 0 && key == MenuKey()) {
			logs::warn("hotkey: it's SKSE Menu Framework's key too, so it does nothing; pick another on the General page");
		}
	}

	std::string KeyName(int a_code)
	{
		if (a_code <= 0 || a_code > 255) {
			return "none";
		}
		const LONG lparam = ((a_code & 0x7F) << 16) | ((a_code & 0x80) ? (1 << 24) : 0);
		char       name[64]{};
		if (GetKeyNameTextA(lparam, name, static_cast<int>(sizeof(name))) > 0) {
			return name;
		}
		return std::format("key {}", a_code);
	}

	int TakePresses()
	{
		return presses.exchange(0);
	}

	// With SKSE Menu Framework's window open the game's input events don't reach
	// us, so the page passes the keys its window sees (ImGuiKey values), mapped to
	// the game's keyboard codes here.
	void Offer(int a_imguiKey)
	{
		if (!capturing) {
			return;
		}
		static constexpr std::pair<int, int> kKeys[]{
			{ 512, 0x0F }, { 513, 0xCB }, { 514, 0xCD }, { 515, 0xC8 }, { 516, 0xD0 }, { 517, 0xC9 }, { 518, 0xD1 },
			{ 519, 0xC7 }, { 520, 0xCF }, { 521, 0xD2 }, { 522, 0xD3 }, { 523, 0x0E }, { 524, 0x39 }, { 525, 0x1C },
			{ 526, 0x01 }, { 527, 0x1D }, { 528, 0x2A }, { 529, 0x38 }, { 530, 0xDB }, { 531, 0x9D }, { 532, 0x36 },
			{ 533, 0xB8 }, { 534, 0xDC }, { 535, 0xDD },
			{ 536, 0x0B }, { 537, 0x02 }, { 538, 0x03 }, { 539, 0x04 }, { 540, 0x05 }, { 541, 0x06 }, { 542, 0x07 },
			{ 543, 0x08 }, { 544, 0x09 }, { 545, 0x0A },
			{ 546, 0x1E }, { 547, 0x30 }, { 548, 0x2E }, { 549, 0x20 }, { 550, 0x12 }, { 551, 0x21 }, { 552, 0x22 },
			{ 553, 0x23 }, { 554, 0x17 }, { 555, 0x24 }, { 556, 0x25 }, { 557, 0x26 }, { 558, 0x32 }, { 559, 0x31 },
			{ 560, 0x18 }, { 561, 0x19 }, { 562, 0x10 }, { 563, 0x13 }, { 564, 0x1F }, { 565, 0x14 }, { 566, 0x16 },
			{ 567, 0x2F }, { 568, 0x11 }, { 569, 0x2D }, { 570, 0x15 }, { 571, 0x2C },
			{ 572, 0x3B }, { 573, 0x3C }, { 574, 0x3D }, { 575, 0x3E }, { 576, 0x3F }, { 577, 0x40 }, { 578, 0x41 },
			{ 579, 0x42 }, { 580, 0x43 }, { 581, 0x44 }, { 582, 0x57 }, { 583, 0x58 }, { 584, 0x64 }, { 585, 0x65 },
			{ 586, 0x66 },
			{ 596, 0x28 }, { 597, 0x33 }, { 598, 0x0C }, { 599, 0x34 }, { 600, 0x35 }, { 601, 0x27 }, { 602, 0x0D },
			{ 603, 0x1A }, { 604, 0x2B }, { 605, 0x1B }, { 606, 0x29 }, { 607, 0x3A }, { 608, 0x46 }, { 609, 0x45 },
			{ 610, 0xB7 }, { 611, 0xC5 },
			{ 612, 0x52 }, { 613, 0x4F }, { 614, 0x50 }, { 615, 0x51 }, { 616, 0x4B }, { 617, 0x4C }, { 618, 0x4D },
			{ 619, 0x47 }, { 620, 0x48 }, { 621, 0x49 }, { 622, 0x53 }, { 623, 0xB5 }, { 624, 0x37 }, { 625, 0x4A },
			{ 626, 0x4E }, { 627, 0x9C }, { 628, 0x8D },
		};
		const auto it = std::ranges::find_if(kKeys, [a_imguiKey](const auto& a_pair) { return a_pair.first == a_imguiKey; });
		if (it == std::ranges::end(kKeys)) {
			return;
		}
		const int key = it->second;
		if (key != static_cast<int>(kEscape) && key == MenuKey()) {
			refused = true;
			return;
		}
		refused = false;
		captured = key == static_cast<int>(kEscape) ? -1 : key;
		capturing = false;
		logs::info("hotkey: the page took {}", key == static_cast<int>(kEscape) ? std::string("Esc (kept the old key)") : KeyName(key));
	}

	void StartCapture()
	{
		refused = false;
		captured = 0;
		capturing = true;
	}

	bool Capturing()
	{
		return capturing;
	}

	bool Refused()
	{
		return refused;
	}

	int TakeCaptured()
	{
		return captured.exchange(0);
	}
}
