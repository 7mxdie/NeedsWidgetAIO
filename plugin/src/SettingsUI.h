#pragma once

// The settings page inside SKSE Menu Framework's F1 menu.
namespace SettingsUI
{
	void Register();       // kPostLoad: the General and Icons pages
	void RegisterNeeds();  // kDataLoaded, after Needs::Init: logs which needs get an Icons section
}
