#pragma once

// The widget's own key (TEST10, Tom: "All but 5" of the ideas - this is idea 2).
// What it does is the General page's choice: hide or show the widget, or show it
// for a while when it has faded out. No key by default. Taken from GTS - Plan or
// Perish's Active Effects key, which Tom has tested: the key is captured from
// ImGui while SKSE Menu Framework's window is open, because the game's own input
// events don't reach plugins then, and SKSE Menu Framework's own key is refused.
namespace Hotkey
{
	void        Init();                 // kDataLoaded
	std::string KeyName(int a_code);    // the key's name on this keyboard
	int         TakePresses();          // presses in play since the last call

	// The page's "press a key" button
	void StartCapture();
	bool Capturing();
	bool Refused();                     // the key pressed was SKSE Menu Framework's own: still waiting
	int  TakeCaptured();                // the key pressed since StartCapture, once; 0 = none yet, -1 = cancelled
	void Offer(int a_imguiKey);         // a key the page's window saw pressed (an ImGuiKey), while capturing
}
