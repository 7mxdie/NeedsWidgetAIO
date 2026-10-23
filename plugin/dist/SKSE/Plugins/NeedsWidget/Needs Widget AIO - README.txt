Needs Widget AIO 2.3.0
by mxdie (Tom)

Shows your needs as icons on the HUD, from whichever needs mods you have:
Survival Mode, SunHelm, Last Seed, iNeed, Frostfall, Stress and Fear, Dirt and
Blood, Bathing in Skyrim, Keep It Clean, DVSMP Survival Tweaks, Tears of Kyne,
Immersive Needs, Realistic Needs and Diseases, Super Simple Needs, Frostbite and
Drink It Off, plus encumbrance from the game.
A need with no mod behind it just doesn't show.

It also comes with four icons that aren't needs:
  Light Level   how bright it is where you stand. From the game itself, so it's off
                until you turn it on in the menu.
  Lantern Fuel  Chesko's Wearable Lanterns oil or torchbug pollen, when fuel is on.
  Wounds        how badly hurt you are, from Wounds.
  Injury        Blade & Blunt's injuries (Starfrost's fourth need).

Other mods can add icons of their own, one ini file each in
SKSE/Plugins/NeedsWidget/Icons/. They show in the same widget and menu.

Settings are in the SKSE Menu Framework menu under "Needs Widget AIO".
Saving there writes Data/SKSE/Plugins/NeedsWidget_Custom.ini.

Needs:
  SKSE, Address Library for SKSE Plugins, SKSE Menu Framework

If something looks off, send me Documents/My Games/Skyrim Special Edition/SKSE/
NeedsWidget.log. It says which mod each icon is reading and why any are hidden.


ICONS
-----
The icons are not AI generated. They were drawn by real people, Lorc and
Delapouite, for game-icons.net, and are used under Creative Commons
Attribution 3.0 (https://creativecommons.org/licenses/by/3.0/).

Changes made: centred, resized to match each other, recoloured white so the
widget can tint them, and given a soft dark edge.

  Hunger          Chicken leg             by Lorc        https://game-icons.net/1x1/lorc/chicken-leg.html
  Cold            Snowflake 2             by Lorc        https://game-icons.net/1x1/lorc/snowflake-2.html
  Fatigue         Night sleep             by Delapouite  https://game-icons.net/1x1/delapouite/night-sleep.html
  Stress          Screaming               by Lorc        https://game-icons.net/1x1/lorc/screaming.html
  Dirt and Blood  Soap                    by Delapouite  https://game-icons.net/1x1/delapouite/soap.html
  Encumbrance     Backpack                by Delapouite  https://game-icons.net/1x1/delapouite/backpack.html
  Thirst          Waterskin               by Delapouite  https://game-icons.net/1x1/delapouite/waterskin.html
  Wetness         Droplets                by Lorc        https://game-icons.net/1x1/lorc/droplets.html
  Vitality        Heart beats             by Delapouite  https://game-icons.net/1x1/delapouite/heart-beats.html
  Disease         Plague doctor profile   by Delapouite  https://game-icons.net/1x1/delapouite/plague-doctor-profile.html
  Light Level     Sun                     by Lorc        https://game-icons.net/1x1/lorc/sun.html
  Lantern Fuel    Old lantern             by Delapouite  https://game-icons.net/1x1/delapouite/old-lantern.html
  Wounds          Bandaged                by Lorc        https://game-icons.net/1x1/lorc/bandaged.html
  Injury          Broken bone             by Lorc        https://game-icons.net/1x1/lorc/broken-bone.html


CODE
----
Needs Widget AIO
Copyright (C) 2026 mxdie

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

The full licence is in LICENSE.txt, next to this file, and at
https://www.gnu.org/licenses/gpl-3.0.html

Source code: https://github.com/7mxdie/NeedsWidgetAIO

The dll is built on CommonLibSSE-NG (GPL-3.0, https://github.com/alandtse/CommonLibSSE-NG),
which is why it's GPL-3.0 too, and the SKSE Menu Framework header (MIT License).
CommonLibSSE-NG grew out of Ryan-rsm-McKenzie's CommonLibSSE, which is under the
MIT License. Their MIT licence texts follow.


--- CommonLibSSE (the original, by Ryan-rsm-McKenzie) ---

MIT License

Copyright (c) 2018 Ryan-rsm-McKenzie

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


--- SKSE Menu Framework ---

MIT License

Copyright (c) 2024 <Ege Yilmaz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
