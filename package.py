"""Package a test or release zip for Vortex.

    python package.py 1.0.0

Builds the SWF and the pictures (with the round-trip check and the Ruffle probe),
checks the dll only calls SKSE Menu Framework functions the installed SMF has,
takes the release DLL from the xmake build, and writes
Release/<label>/NeedsWidgetAIO-<label>.zip containing exactly:

    SKSE/Plugins/NeedsWidget.dll
    SKSE/Plugins/NeedsWidget.ini
    SKSE/Plugins/NeedsWidget/Presets/Vel'dun.ini
    SKSE/Plugins/NeedsWidget/Presets/Norden.ini
    SKSE/Plugins/NeedsWidget/Needs Widget AIO - README.txt
    SKSE/Plugins/NeedsWidget/LICENSE.txt
    SKSE/Plugins/NeedsWidget/Icons/<the shipped extra icons>.ini
    SKSE/Plugins/NeedsWidget/Styles/<the 49 icon styles>.ini
    Interface/NeedsWidget/widget.swf
    Interface/NeedsWidget/icons/<the glyphs, two frames and the styles' frames>.swf

Then reopens the zip and checks every entry against its source by SHA-256.
"""

import hashlib
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ICONS = os.path.join(HERE, "swf", "build", "Interface", "NeedsWidget", "icons")
PICTURES = ["hunger", "cold", "fatigue", "stress", "dirt", "encumbrance", "thirst", "wetness", "vitality", "disease",
            "light", "lantern", "wounds", "injury", "vampire", "magicdebuffs", "lute", "flute", "drum", "strength", "agility", "intelligence", "temperature", "werewolf", "homesick", "frame_ring", "frame_bar"]
# 3.0: the alternative pictures players can pick for any icon (swf/art/alternatives.py).
sys.path.insert(0, os.path.join(HERE, "swf", "art"))
import alternatives  # noqa: E402
PICTURES = PICTURES + list(alternatives.ALTERNATIVES)
# Extra icons the main mod ships (docs/EXTRA-ICONS.md); their art is in PICTURES.
EXTRA_ICONS = ["Light Level", "Lantern Fuel", "Wounds", "Injury", "Vampire", "Magic Debuffs", "Bard - Lute", "Bard - Flute", "Bard - Drum", "Strength", "Agility", "Intelligence", "Temperature", "Werewolf", "Homesick", "Disease 2", "Disease 3", "Disease 4", "Disease 5", "Disease 6"]
FILES = {
    "SKSE/Plugins/NeedsWidget.dll": os.path.join(HERE, "plugin", "build", "windows", "x64", "release", "NeedsWidget.dll"),
    "SKSE/Plugins/NeedsWidget.ini": os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget.ini"),
    "SKSE/Plugins/NeedsWidget/Presets/Vel'dun.ini": os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "Presets", "Vel'dun.ini"),
    "SKSE/Plugins/NeedsWidget/Presets/Norden.ini": os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "Presets", "Norden.ini"),
    "SKSE/Plugins/NeedsWidget/Needs Widget AIO - README.txt": os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "Needs Widget AIO - README.txt"),
    "SKSE/Plugins/NeedsWidget/LICENSE.txt": os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "LICENSE.txt"),
    "Interface/NeedsWidget/widget.swf": os.path.join(HERE, "swf", "build", "Interface", "NeedsWidget", "widget.swf"),
}
for _name in EXTRA_ICONS:
    FILES["SKSE/Plugins/NeedsWidget/Icons/%s.ini" % _name] = os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "Icons", _name + ".ini")
for _name in PICTURES:
    FILES["Interface/NeedsWidget/icons/%s.swf" % _name] = os.path.join(ICONS, _name + ".swf")
# 3.0 icon styles (swf/styles/style_defs.py): each one's data file, and its frame unless it has none.
sys.path.insert(0, os.path.join(HERE, "swf", "styles"))
import style_defs  # noqa: E402
STYLE_DIR = os.path.join(HERE, "swf", "build", "SKSE", "Plugins", "NeedsWidget", "Styles")
for _style in style_defs.STYLES:
    FILES["SKSE/Plugins/NeedsWidget/Styles/%s.ini" % _style["key"]] = os.path.join(STYLE_DIR, _style["key"] + ".ini")
    if _style["frame"]:
        FILES["Interface/NeedsWidget/icons/frame_%s.swf" % _style["key"]] = os.path.join(ICONS, "frame_%s.swf" % _style["key"])


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    label = sys.argv[1]

    subprocess.run([sys.executable, os.path.join(HERE, "swf", "build_widget.py"), "--probe"], check=True)
    subprocess.run([sys.executable, os.path.join(HERE, "plugin", "check_smf_exports.py")], check=True)
    for arc, src in FILES.items():
        if not os.path.isfile(src):
            sys.exit("missing: %s" % src)

    out_dir = os.path.join(HERE, "Release", label)
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "NeedsWidgetAIO-%s.zip" % label)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for arc, src in FILES.items():
            z.write(src, arc)

    with zipfile.ZipFile(out) as z:
        names = z.namelist()
        if sorted(names) != sorted(FILES):
            sys.exit("zip contents wrong: %s" % names)
        for arc, src in FILES.items():
            if sha256(z.read(arc)) != sha256(open(src, "rb").read()):
                sys.exit("zip entry differs from source: %s" % arc)
            print("  %-44s %s" % (arc, sha256(z.read(arc))[:16]))

    data = open(out, "rb").read()
    print("%s  %d bytes  sha256 %s" % (out, len(data), sha256(data)))


if __name__ == "__main__":
    main()
