#include "Extras.h"
#include "Hotkey.h"
#include "HudLayer.h"
#include "Needs.h"
#include "Settings.h"
#include "SettingsUI.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			// Every plugin is loaded now, SKSE Menu Framework included. Extra icons
			// first: the settings are saved under their names.
			Extras::Scan();
			Settings::Load();
			SettingsUI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Needs::Init();
			SettingsUI::RegisterNeeds();
			HudLayer::Register();
			Hotkey::Init();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			HudLayer::Show();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	logs::info("NeedsWidget {} loaded", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string());
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
