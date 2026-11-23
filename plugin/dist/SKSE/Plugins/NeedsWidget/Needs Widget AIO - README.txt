Needs Widget AIO 3.0.0
by mxdie (Tom)

Shows your needs as icons on the HUD, from whichever needs mods you have:
Survival Mode, SunHelm, Last Seed, iNeed, Frostfall, Stress and Fear, Dirt and
Blood, Bathing in Skyrim, Keep It Clean, DVSMP Survival Tweaks, Tears of Kyne,
Immersive Needs, Realistic Needs and Diseases, Super Simple Needs, Simple Needs
Expanded, Vitality Mode, Frostbite (.esp or .esl) and Drink It Off, plus encumbrance
from the game.
A need with no mod behind it just doesn't show.

It also comes with icons that aren't needs:
  Light Level   how bright it is where you stand. From the game itself, so it's off
                until you turn it on in the menu.
  Lantern Fuel  Chesko's Wearable Lanterns oil or torchbug pollen, when fuel is on, or
                Fuelable Lanterns for Quick Light.
  Wounds        how badly hurt you are, from Wounds.
  Injury        Blade & Blunt's injuries (Starfrost's fourth need), or Rogues 'n Raiders'.
  Vampire       your vampire stage, 1 to 4. Hidden unless you're a vampire.
  Werewolf      seconds left in beast form. Only shows while you're transformed.
  Magic Debuffs Rogues 'n Raiders' hexes, sorceries and curses.
  Bard          Skyrim's Got Talent: your lute, flute and drum skill, one icon each,
                hidden once you're a Pro.
  Strength, Agility, Intelligence
                Strength Matters, Agility Matters and Intelligence Matters: the stat
                itself, one icon each, the number is the stat.
  Temperature   how cold the air is: Survival Mode (with or without Survival Mode
                Improved) or Frostfall. Cold is how cold you are; this is the weather.
  Homesick      how long since you were last in a player home, from Simple Needs
                Expanded. Hidden until it has seen you in one.
  Disease 2-6   with "One icon per disease" on (Disease's section on the Icons page),
                each further disease gets its own icon, named. Off, they aren't there.

Other mods can add icons of their own, one ini file each in
SKSE/Plugins/NeedsWidget/Icons/. They show in the same widget and menu.

Settings are in the SKSE Menu Framework menu under "Needs Widget AIO".
Saving there writes Data/SKSE/Plugins/NeedsWidget_Custom.ini.
"Drag icons on screen" (its own tab, and at the top of the pages) lets you
move and resize the icons with the mouse.
"Show the widget", at the top of the General page, hides every icon at once
and is kept straight away; a hotkey set to hide and show flips the same switch.
Each icon's section can give it any other picture from the icons folder, and say
where "fine" ends for Hide when fine.
The General page's Look > Style picks how every icon looks: the classic Ring, Bar,
Icon only and Fill, or one of 49 styles, each with its own frame, meter and colours
(Nordic shield, Dwemer, Parchment, Map marker, Alchemy flask, Dark Brotherhood...).
Styles are files in SKSE/Plugins/NeedsWidget/Styles/, so other mods can add more.
Every icon can also show any of 131 other pictures (Icons page, Picture: grouped
by icon, with a search box).

Also new in 3.0:
  - Worst first: a row or column lines up by how bad each need is; Show at most N
    keeps only the worst few; icons can grow at their worst two stages.
  - A second row or column: send any icon there from its section.
  - When to show: hide in combat, with a weapon out, or while riding; each icon
    can show only outdoors or indoors.
  - A soft flash when a need gets better.
  - A trend arrow, stage pips, a Blue to orange colour scheme for colour-blind
    players, and numbers that can show game hours until the worst stage.
  - Copy my settings / Paste a preset: share a whole setup as text.

Needs:
  SKSE, Address Library for SKSE Plugins, SKSE Menu Framework

If something looks off, send me Documents/My Games/Skyrim Special Edition/SKSE/
NeedsWidget.log. It says which mod each icon is reading and why any are hidden.


ICONS
-----
The icons are not AI generated. They were drawn by real people, Lorc,
Delapouite, Skoll, Sbed, Zajkonur and Caro Asercion, for game-icons.net, and are used under Creative Commons
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
  Vampire         Vampire Dracula         by Delapouite  https://game-icons.net/1x1/delapouite/vampire-dracula.html
  Magic Debuffs   Cursed star             by Lorc        https://game-icons.net/1x1/lorc/cursed-star.html
  Bard - Lute     Lyre                    by Lorc        https://game-icons.net/1x1/lorc/lyre.html
  Bard - Flute    Flute                   by Delapouite  https://game-icons.net/1x1/delapouite/flute.html
  Bard - Drum     Drum                    by Delapouite  https://game-icons.net/1x1/delapouite/drum.html
  Strength        Biceps                  by Delapouite  https://game-icons.net/1x1/delapouite/biceps.html
  Agility         Sprint                  by Lorc        https://game-icons.net/1x1/lorc/sprint.html
  Homesick        House                   by Delapouite  https://game-icons.net/1x1/delapouite/house.html
  Intelligence    Brain                   by Lorc        https://game-icons.net/1x1/lorc/brain.html
  Temperature     Thermometer scale       by Lorc        https://game-icons.net/1x1/lorc/thermometer-scale.html
  Werewolf        Werewolf                by Lorc        https://game-icons.net/1x1/lorc/werewolf.html

Alternative pictures, picked per icon on the Icons page (same changes):

  Hunger: meat                  Meat                    by Lorc          https://game-icons.net/1x1/lorc/meat.html
  Hunger: ham shank             Ham shank               by Skoll         https://game-icons.net/1x1/skoll/ham-shank.html
  Hunger: roast chicken         Roast chicken           by Lorc          https://game-icons.net/1x1/lorc/roast-chicken.html
  Hunger: bread                 Bread                   by Delapouite    https://game-icons.net/1x1/delapouite/bread.html
  Hunger: sliced bread          Sliced bread            by Lorc          https://game-icons.net/1x1/lorc/sliced-bread.html
  Hunger: cheese wedge          Cheese wedge            by Lorc          https://game-icons.net/1x1/lorc/cheese-wedge.html
  Hunger: stomach               Stomach                 by Delapouite    https://game-icons.net/1x1/delapouite/stomach.html
  Hunger: knife fork            Knife fork              by Lorc          https://game-icons.net/1x1/lorc/knife-fork.html
  Hunger: shiny apple           Shiny apple             by Lorc          https://game-icons.net/1x1/lorc/shiny-apple.html
  Cold: snowflake 1             Snowflake 1             by Lorc          https://game-icons.net/1x1/lorc/snowflake-1.html
  Cold: tension snowflake       Tension snowflake       by Lorc          https://game-icons.net/1x1/lorc/tension-snowflake.html
  Cold: thermometer cold        Thermometer cold        by Delapouite    https://game-icons.net/1x1/delapouite/thermometer-cold.html
  Cold: cold heart              Cold heart              by Lorc          https://game-icons.net/1x1/lorc/cold-heart.html
  Cold: frozen body             Frozen body             by Delapouite    https://game-icons.net/1x1/delapouite/frozen-body.html
  Cold: snowing                 Snowing                 by Lorc          https://game-icons.net/1x1/lorc/snowing.html
  Cold: frozen orb              Frozen orb              by Lorc          https://game-icons.net/1x1/lorc/frozen-orb.html
  Cold: brain freeze            Brain freeze            by Lorc          https://game-icons.net/1x1/lorc/brain-freeze.html
  Cold: icebergs                Icebergs                by Lorc          https://game-icons.net/1x1/lorc/icebergs.html
  Fatigue: bed                  Bed                     by Delapouite    https://game-icons.net/1x1/delapouite/bed.html
  Fatigue: person in bed        Person in bed           by Delapouite    https://game-icons.net/1x1/delapouite/person-in-bed.html
  Fatigue: sleepy               Sleepy                  by Lorc          https://game-icons.net/1x1/lorc/sleepy.html
  Fatigue: moon                 Moon                    by Lorc          https://game-icons.net/1x1/lorc/moon.html
  Fatigue: night sky            Night sky               by Lorc          https://game-icons.net/1x1/lorc/night-sky.html
  Fatigue: tired eye            Tired eye               by Delapouite    https://game-icons.net/1x1/delapouite/tired-eye.html
  Fatigue: sleeping bag         Sleeping bag            by Delapouite    https://game-icons.net/1x1/delapouite/sleeping-bag.html
  Fatigue: candle flame         Candle flame            by Lorc          https://game-icons.net/1x1/lorc/candle-flame.html
  Fatigue: bed lamp             Bed lamp                by Delapouite    https://game-icons.net/1x1/delapouite/bed-lamp.html
  Stress: despair               Despair                 by Lorc          https://game-icons.net/1x1/lorc/despair.html
  Stress: terror                Terror                  by Lorc          https://game-icons.net/1x1/lorc/terror.html
  Stress: worried eyes          Worried eyes            by Lorc          https://game-icons.net/1x1/lorc/worried-eyes.html
  Stress: brain                 Brain                   by Lorc          https://game-icons.net/1x1/lorc/brain.html
  Stress: brainstorm            Brainstorm              by Lorc          https://game-icons.net/1x1/lorc/brainstorm.html
  Stress: brain leak            Brain leak              by Delapouite    https://game-icons.net/1x1/delapouite/brain-leak.html
  Stress: skull crack           Skull crack             by Lorc          https://game-icons.net/1x1/lorc/skull-crack.html
  Stress: broken skull          Broken skull            by Lorc          https://game-icons.net/1x1/lorc/broken-skull.html
  Dirt: bathtub                 Bathtub                 by Delapouite    https://game-icons.net/1x1/delapouite/bathtub.html
  Dirt: liquid soap             Liquid soap             by Delapouite    https://game-icons.net/1x1/delapouite/liquid-soap.html
  Dirt: soap experiment         Soap experiment         by Lorc          https://game-icons.net/1x1/lorc/soap-experiment.html
  Dirt: full wood bucket        Full wood bucket        by Delapouite    https://game-icons.net/1x1/delapouite/full-wood-bucket.html
  Dirt: empty wood bucket       Empty wood bucket       by Delapouite    https://game-icons.net/1x1/delapouite/empty-wood-bucket.html
  Dirt: shower                  Shower                  by Delapouite    https://game-icons.net/1x1/delapouite/shower.html
  Dirt: bleeding heart          Bleeding heart          by Lorc          https://game-icons.net/1x1/lorc/bleeding-heart.html
  Encumbrance: knapsack         Knapsack                by Lorc          https://game-icons.net/1x1/lorc/knapsack.html
  Encumbrance: light backpack   Light backpack          by Delapouite    https://game-icons.net/1x1/delapouite/light-backpack.html
  Encumbrance: weight           Weight                  by Delapouite    https://game-icons.net/1x1/delapouite/weight.html
  Encumbrance: weight crush     Weight crush            by Sbed          https://game-icons.net/1x1/sbed/weight-crush.html
  Encumbrance: chest            Chest                   by Delapouite    https://game-icons.net/1x1/delapouite/chest.html
  Encumbrance: open chest       Open chest              by Skoll         https://game-icons.net/1x1/skoll/open-chest.html
  Encumbrance: shoulder bag     Shoulder bag            by Delapouite    https://game-icons.net/1x1/delapouite/shoulder-bag.html
  Encumbrance: weight scale     Weight scale            by Delapouite    https://game-icons.net/1x1/delapouite/weight-scale.html
  Thirst: water flask           Water flask             by Delapouite    https://game-icons.net/1x1/delapouite/water-flask.html
  Thirst: jug                   Jug                     by Delapouite    https://game-icons.net/1x1/delapouite/jug.html
  Thirst: drinking              Drinking                by Delapouite    https://game-icons.net/1x1/delapouite/drinking.html
  Thirst: well                  Well                    by Delapouite    https://game-icons.net/1x1/delapouite/well.html
  Thirst: round bottom flask    Round bottom flask      by Lorc          https://game-icons.net/1x1/lorc/round-bottom-flask.html
  Thirst: wine bottle           Wine bottle             by Delapouite    https://game-icons.net/1x1/delapouite/wine-bottle.html
  Thirst: mouth watering        Mouth watering          by Lorc          https://game-icons.net/1x1/lorc/mouth-watering.html
  Thirst: water drop            Water drop              by Sbed          https://game-icons.net/1x1/sbed/water-drop.html
  Thirst: full metal bucket     Full metal bucket       by Delapouite    https://game-icons.net/1x1/delapouite/full-metal-bucket.html
  Wetness: drop                 Drop                    by Lorc          https://game-icons.net/1x1/lorc/drop.html
  Wetness: raining              Raining                 by Lorc          https://game-icons.net/1x1/lorc/raining.html
  Wetness: heavy rain           Heavy rain              by Lorc          https://game-icons.net/1x1/lorc/heavy-rain.html
  Wetness: splash               Splash                  by Lorc          https://game-icons.net/1x1/lorc/splash.html
  Wetness: water splash         Water splash            by Lorc          https://game-icons.net/1x1/lorc/water-splash.html
  Wetness: grouped drops        Grouped drops           by Lorc          https://game-icons.net/1x1/lorc/grouped-drops.html
  Wetness: umbrella             Umbrella                by Lorc          https://game-icons.net/1x1/lorc/umbrella.html
  Wetness: droplet splash       Droplet splash          by Lorc          https://game-icons.net/1x1/lorc/droplet-splash.html
  Wetness: splash 2             Splash                  by Sbed          https://game-icons.net/1x1/sbed/splash.html
  Vitality: heart organ         Heart organ             by Lorc          https://game-icons.net/1x1/lorc/heart-organ.html
  Vitality: life support        Life support            by Lorc          https://game-icons.net/1x1/lorc/life-support.html
  Vitality: health potion       Health potion           by Delapouite    https://game-icons.net/1x1/delapouite/health-potion.html
  Vitality: shining heart       Shining heart           by Lorc          https://game-icons.net/1x1/lorc/shining-heart.html
  Vitality: wrapped heart       Wrapped heart           by Lorc          https://game-icons.net/1x1/lorc/wrapped-heart.html
  Vitality: heart bottle        Heart bottle            by Lorc          https://game-icons.net/1x1/lorc/heart-bottle.html
  Disease: virus                Virus                   by Lorc          https://game-icons.net/1x1/lorc/virus.html
  Disease: vomiting             Vomiting                by Lorc          https://game-icons.net/1x1/lorc/vomiting.html
  Disease: poison cloud         Poison cloud            by Sbed          https://game-icons.net/1x1/sbed/poison-cloud.html
  Disease: poison gas           Poison gas              by Lorc          https://game-icons.net/1x1/lorc/poison-gas.html
  Disease: poison bottle        Poison bottle           by Lorc          https://game-icons.net/1x1/lorc/poison-bottle.html
  Disease: goo skull            Goo skull               by Lorc          https://game-icons.net/1x1/lorc/goo-skull.html
  Disease: leaky skull          Leaky skull             by Lorc          https://game-icons.net/1x1/lorc/leaky-skull.html
  Disease: skull with syringe   Skull with syringe      by Zajkonur      https://game-icons.net/1x1/zajkonur/skull-with-syringe.html
  Light: sunrise                Sunrise                 by Lorc          https://game-icons.net/1x1/lorc/sunrise.html
  Light: sunbeams               Sunbeams                by Lorc          https://game-icons.net/1x1/lorc/sunbeams.html
  Light: light bulb             Light bulb              by Lorc          https://game-icons.net/1x1/lorc/light-bulb.html
  Light: torch                  Torch                   by Delapouite    https://game-icons.net/1x1/delapouite/torch.html
  Light: candlebright           Candlebright            by Lorc          https://game-icons.net/1x1/lorc/candlebright.html
  Lantern: lantern              Lantern                 by Lorc          https://game-icons.net/1x1/lorc/lantern.html
  Lantern: lantern flame        Lantern flame           by Lorc          https://game-icons.net/1x1/lorc/lantern-flame.html
  Lantern: fire bowl            Fire bowl               by Lorc          https://game-icons.net/1x1/lorc/fire-bowl.html
  Wounds: bandage roll          Bandage roll            by Lorc          https://game-icons.net/1x1/lorc/bandage-roll.html
  Wounds: bleeding wound        Bleeding wound          by Lorc          https://game-icons.net/1x1/lorc/bleeding-wound.html
  Wounds: bloody stash          Bloody stash            by Lorc          https://game-icons.net/1x1/lorc/bloody-stash.html
  Injury: arm sling             Arm sling               by Delapouite    https://game-icons.net/1x1/delapouite/arm-sling.html
  Injury: crossed bones         Crossed bones           by Lorc          https://game-icons.net/1x1/lorc/crossed-bones.html
  Injury: leg                   Leg                     by Delapouite    https://game-icons.net/1x1/delapouite/leg.html
  Vampire: vampire cape         Vampire cape            by Delapouite    https://game-icons.net/1x1/delapouite/vampire-cape.html
  Vampire: fangs                Fangs                   by Skoll         https://game-icons.net/1x1/skoll/fangs.html
  Vampire: fangs circle         Fangs circle            by Lorc          https://game-icons.net/1x1/lorc/fangs-circle.html
  Vampire: bat                  Bat                     by Delapouite    https://game-icons.net/1x1/delapouite/bat.html
  Vampire: fanged skull         Fanged skull            by Lorc          https://game-icons.net/1x1/lorc/fanged-skull.html
  Magic debuffs: magic swirl    Magic swirl             by Lorc          https://game-icons.net/1x1/lorc/magic-swirl.html
  Magic debuffs: magic palm     Magic palm              by Lorc          https://game-icons.net/1x1/lorc/magic-palm.html
  Magic debuffs: hexes          Hexes                   by Skoll         https://game-icons.net/1x1/skoll/hexes.html
  Magic debuffs: spell book     Spell book              by Delapouite    https://game-icons.net/1x1/delapouite/spell-book.html
  Lute: guitar                  Guitar                  by Lorc          https://game-icons.net/1x1/lorc/guitar.html
  Lute: harp                    Harp                    by Delapouite    https://game-icons.net/1x1/delapouite/harp.html
  Flute: ocarina                Ocarina                 by Delapouite    https://game-icons.net/1x1/delapouite/ocarina.html
  Flute: pan flute              Pan flute               by Delapouite    https://game-icons.net/1x1/delapouite/pan-flute.html
  Drum: drum kit                Drum kit                by Caro Asercion https://game-icons.net/1x1/caro-asercion/drum-kit.html
  Strength: muscle up           Muscle up               by Lorc          https://game-icons.net/1x1/lorc/muscle-up.html
  Strength: strong              Strong                  by Lorc          https://game-icons.net/1x1/lorc/strong.html
  Strength: weight lifting up   Weight lifting up       by Delapouite    https://game-icons.net/1x1/delapouite/weight-lifting-up.html
  Agility: run                  Run                     by Lorc          https://game-icons.net/1x1/lorc/run.html
  Agility: feather              Feather                 by Lorc          https://game-icons.net/1x1/lorc/feather.html
  Agility: wingfoot             Wingfoot                by Lorc          https://game-icons.net/1x1/lorc/wingfoot.html
  Intelligence: brain tentacle  Brain tentacle          by Delapouite    https://game-icons.net/1x1/delapouite/brain-tentacle.html
  Intelligence: scroll unfurled Scroll unfurled         by Lorc          https://game-icons.net/1x1/lorc/scroll-unfurled.html
  Intelligence: book cover      Book cover              by Lorc          https://game-icons.net/1x1/lorc/book-cover.html
  Temperature: thermometer hot  Thermometer hot         by Delapouite    https://game-icons.net/1x1/delapouite/thermometer-hot.html
  Temperature: thermometer cold Thermometer cold        by Delapouite    https://game-icons.net/1x1/delapouite/thermometer-cold.html
  Temperature: campfire         Campfire                by Lorc          https://game-icons.net/1x1/lorc/campfire.html
  Werewolf: wolf head           Wolf head               by Lorc          https://game-icons.net/1x1/lorc/wolf-head.html
  Werewolf: wolf howl           Wolf howl               by Lorc          https://game-icons.net/1x1/lorc/wolf-howl.html
  Werewolf: moon claws          Moon claws              by Lorc          https://game-icons.net/1x1/lorc/moon-claws.html
  Werewolf: claw slashes        Claw slashes            by Lorc          https://game-icons.net/1x1/lorc/claw-slashes.html
  Homesick: village             Village                 by Delapouite    https://game-icons.net/1x1/delapouite/village.html
  Homesick: family house        Family house            by Delapouite    https://game-icons.net/1x1/delapouite/family-house.html
  Homesick: castle              Castle                  by Lorc          https://game-icons.net/1x1/lorc/castle.html
  Homesick: house keys          House keys              by Delapouite    https://game-icons.net/1x1/delapouite/house-keys.html
  Homesick: fireplace           Fireplace               by Delapouite    https://game-icons.net/1x1/delapouite/fireplace.html


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
