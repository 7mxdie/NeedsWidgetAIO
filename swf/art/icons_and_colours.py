"""Draw the main mod's Nexus image: every icon in its six stages, in both colour sets,
the way the widget draws them in the Ring look.

    python swf/art/icons_and_colours.py

Exports each picture from the built swfs with FFDec at 8x (64 units -> 512 px), then
writes Release/nexus/icons_and_colours.png. Run package.py first so swf/build is current.
"""

import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FFDEC = r"C:\Program Files (x86)\FFDec\ffdec-cli.exe"
ICONS = os.path.join(ROOT, "swf", "build", "Interface", "NeedsWidget", "icons")
OUT = os.path.join(ROOT, "Release", "nexus", "icons_and_colours.png")
GEORGIA = r"C:/Windows/Fonts/georgia.ttf"
NEEDS = [("hunger", "Hunger"), ("thirst", "Thirst"), ("cold", "Cold"), ("wetness", "Wetness"), ("fatigue", "Fatigue"),
         ("vitality", "Vitality"), ("disease", "Disease"), ("stress", "Stress"), ("dirt", "Dirt and Blood"),
         ("encumbrance", "Encumbrance"), ("wounds", "Wounds"), ("injury", "Injury"), ("lantern", "Lantern Fuel"),
         ("light", "Light Level"), ("magicdebuffs", "Magic Debuffs"), ("vampire", "Vampire"), ("lute", "Bard - Lute"),
         ("flute", "Bard - Flute"), ("drum", "Bard - Drum"), ("strength", "Strength"), ("agility", "Agility"), ("intelligence", "Intelligence")]
SCHEMES = [("Bone to red", [0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F]),
           ("Green to red", [0x6FBF5A, 0xA9C94E, 0xE3C94A, 0xE8A060, 0xD9503F, 0xFF2E1F])]
FILL = [0.10, 0.28, 0.46, 0.64, 0.82, 1.0]
U = 8  # pixels per widget unit in the tiles


def export(tmp, name):
    out = os.path.join(tmp, name)
    subprocess.run([FFDEC, "-format", "frame:png", "-zoom", "8", "-export", "frame", out,
                    os.path.join(ICONS, name + ".swf")], check=True, capture_output=True)
    return Image.open(os.path.join(out, "1.png")).convert("RGBA")


def rgb(c):
    return (c >> 16 & 255, c >> 8 & 255, c & 255)


# frame_ring, the meter band (radius 27.5, width 1.8, clockwise from 12 o'clock), the
# glyph at 42 of 64 units, multiplied by the stage colour (widget.as applyTint).
def tile(frame, art, color, f):
    t = frame.copy()
    meter = Image.new("L", t.size, 0)
    md = ImageDraw.Draw(meter)
    c, ro, ri = 256, 28.4 * U, 26.6 * U
    md.pieslice([c - ro, c - ro, c + ro, c + ro], -90, -90 + 360 * f, fill=255)
    md.ellipse([c - ri, c - ri, c + ri, c + ri], fill=0)
    t.alpha_composite(Image.composite(Image.new("RGBA", t.size, rgb(color) + (255,)), Image.new("RGBA", t.size, (0, 0, 0, 0)), meter))
    a = art.resize((42 * U, 42 * U), Image.LANCZOS)
    r, g, b, al = a.split()
    cr, cg, cb = rgb(color)
    a = Image.merge("RGBA", (r.point(lambda v: v * cr // 255), g.point(lambda v: v * cg // 255), b.point(lambda v: v * cb // 255), al))
    t.alpha_composite(a, (c - 21 * U, c - 21 * U))
    return t


def main():
    with tempfile.TemporaryDirectory() as tmp:
        frame = export(tmp, "frame_ring")
        art = {n: export(tmp, n) for n, _ in NEEDS}
    K = 2  # drawn at 2x, then halved
    # 1080 high fitted the first 14 rows; more rows make it taller (2.6.0: 19).
    H = max(1080, 160 + (len(NEEDS) - 1) * 66 + 70)
    sheet = Image.new("RGBA", (1920 * K, H * K), (0, 0, 0, 255))
    d = ImageDraw.Draw(sheet)
    name_font = ImageFont.truetype(GEORGIA, 27 * K)
    head_font = ImageFont.truetype(GEORGIA, 44 * K)
    small_font = ImageFont.truetype(GEORGIA, 24 * K)
    ICON, STEP, ROW = 60 * K, 104 * K, 66 * K
    GROUP_X = [470 * K, 1180 * K]
    TOP = 160 * K
    for gi, (title, colors) in enumerate(SCHEMES):
        x0 = GROUP_X[gi]
        d.text((x0 + STEP * 2.5, 44 * K), title, font=head_font, fill="white", anchor="mm")
        d.text((x0, 102 * K), "fine", font=small_font, fill=(170, 170, 170), anchor="mm")
        d.text((x0 + STEP * 5, 102 * K), "worst", font=small_font, fill=(170, 170, 170), anchor="mm")
        for ri, (n, _) in enumerate(NEEDS):
            cy = TOP + ri * ROW
            for s in range(6):
                im = tile(frame, art[n], colors[s], FILL[s]).resize((ICON, ICON), Image.LANCZOS)
                sheet.alpha_composite(im, (int(x0 + s * STEP - ICON / 2), int(cy - ICON / 2)))
    for ri, (n, label) in enumerate(NEEDS):
        d.text((400 * K, TOP + ri * ROW), label, font=name_font, fill="white", anchor="rm")
    sheet.convert("RGB").resize((1920, H), Image.LANCZOS).save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
