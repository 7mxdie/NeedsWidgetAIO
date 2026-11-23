"""Alternative pictures players can pick for any icon (3.0, Tom 2026-11-23: "add the icon selection for users
too", alphabetical with a search bar, to cross reference the mod page).

The candidates from art-options/alternatives.png (alternatives_sheet.py), every one shipped. Each becomes
icons/<need>_<icon>.swf, so the picker lists them grouped by need ("Hunger: ham shank"). Sources are
game-icons.net (CC BY 3.0) in game-icons/, credited in game-icons/README.md and the release README.
"""

NEEDS = [
    ("hunger", ["lorc_meat", "skoll_ham-shank", "lorc_roast-chicken", "delapouite_bread", "lorc_sliced-bread",
                "lorc_cheese-wedge", "delapouite_stomach", "lorc_knife-fork", "lorc_shiny-apple"]),
    ("cold", ["lorc_snowflake-1", "lorc_tension-snowflake", "delapouite_thermometer-cold", "lorc_cold-heart",
              "delapouite_frozen-body", "lorc_snowing", "lorc_frozen-orb", "lorc_brain-freeze", "lorc_icebergs"]),
    ("fatigue", ["delapouite_bed", "delapouite_person-in-bed", "lorc_sleepy", "lorc_moon", "lorc_night-sky",
                 "delapouite_tired-eye", "delapouite_sleeping-bag", "lorc_candle-flame", "delapouite_bed-lamp"]),
    ("stress", ["lorc_despair", "lorc_terror", "lorc_worried-eyes", "lorc_brain", "lorc_brainstorm",
                "delapouite_brain-leak", "lorc_skull-crack", "lorc_broken-skull"]),
    ("dirt", ["delapouite_bathtub", "delapouite_liquid-soap", "lorc_soap-experiment", "delapouite_full-wood-bucket",
              "delapouite_empty-wood-bucket", "delapouite_shower", "lorc_bleeding-heart"]),
    ("encumbrance", ["lorc_knapsack", "delapouite_light-backpack", "delapouite_weight", "sbed_weight-crush",
                     "delapouite_chest", "skoll_open-chest", "delapouite_shoulder-bag", "delapouite_weight-scale"]),
    ("thirst", ["delapouite_water-flask", "delapouite_jug", "delapouite_drinking", "delapouite_well",
                "lorc_round-bottom-flask", "delapouite_wine-bottle", "lorc_mouth-watering", "sbed_water-drop",
                "delapouite_full-metal-bucket"]),
    ("wetness", ["lorc_drop", "lorc_raining", "lorc_heavy-rain", "lorc_splash", "lorc_water-splash",
                 "lorc_grouped-drops", "lorc_umbrella", "lorc_droplet-splash", "sbed_splash"]),
    ("vitality", ["lorc_heart-organ", "lorc_life-support", "delapouite_health-potion", "lorc_shining-heart",
                  "lorc_wrapped-heart", "lorc_heart-bottle"]),
    ("disease", ["lorc_virus", "lorc_vomiting", "sbed_poison-cloud", "lorc_poison-gas", "lorc_poison-bottle",
                 "lorc_goo-skull", "lorc_leaky-skull", "zajkonur_skull-with-syringe"]),
    # The extra icons the main mod ships (Tom, 2026-11-23: more picture options), named by their own art name.
    ("light", ["lorc_sunrise", "lorc_sunbeams", "lorc_light-bulb", "delapouite_torch", "lorc_candlebright"]),
    ("lantern", ["lorc_lantern", "lorc_lantern-flame", "lorc_fire-bowl"]),
    ("wounds", ["lorc_bandage-roll", "lorc_bleeding-wound", "lorc_bloody-stash"]),
    ("injury", ["delapouite_arm-sling", "lorc_crossed-bones", "delapouite_leg"]),
    ("vampire", ["delapouite_vampire-cape", "skoll_fangs", "lorc_fangs-circle", "delapouite_bat", "lorc_fanged-skull"]),
    ("magicdebuffs", ["lorc_magic-swirl", "lorc_magic-palm", "skoll_hexes", "delapouite_spell-book"]),
    ("lute", ["lorc_guitar", "delapouite_harp"]),
    ("flute", ["delapouite_ocarina", "delapouite_pan-flute"]),
    ("drum", ["caro-asercion_drum-kit"]),
    ("strength", ["lorc_muscle-up", "lorc_strong", "delapouite_weight-lifting-up"]),
    ("agility", ["lorc_run", "lorc_feather", "lorc_wingfoot"]),
    ("intelligence", ["delapouite_brain-tentacle", "lorc_scroll-unfurled", "lorc_book-cover"]),
    ("temperature", ["delapouite_thermometer-hot", "delapouite_thermometer-cold", "lorc_campfire"]),
    ("werewolf", ["lorc_wolf-head", "lorc_wolf-howl", "lorc_moon-claws", "lorc_claw-slashes"]),
    ("homesick", ["delapouite_village", "delapouite_family-house", "lorc_castle", "delapouite_house-keys", "delapouite_fireplace"]),
]

ARTISTS = {"lorc": "Lorc", "delapouite": "Delapouite", "skoll": "Skoll", "sbed": "Sbed", "zajkonur": "Zajkonur",
           "caro-asercion": "Caro Asercion"}

# picture name -> game-icons source ("author_icon")
ALTERNATIVES = {}
for _need, _sources in NEEDS:
    for _src in _sources:
        _name = f"{_need}_{_src.split('_', 1)[1]}"
        while _name in ALTERNATIVES:  # two artists' "splash": the second is splash-2
            _name = _name + "-2" if not _name[-1].isdigit() else _name[:-1] + str(int(_name[-1]) + 1)
        ALTERNATIVES[_name] = _src
assert len(ALTERNATIVES) == sum(len(s) for _, s in NEEDS)
