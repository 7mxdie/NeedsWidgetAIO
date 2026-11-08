"""Draw the main mod's looks image: ten icons in each look (Ring, Bar, Icon only, Fill),
the way widget.as draws them.

    python swf/art/looks.py

Uses icons_and_colours.py's FFDec export. Run package.py first so swf/build is current.
Writes Release/nexus/looks.png. 2.9.0 added the Fill row; the first image (Ring, Bar,
Icon only) was put together by hand and is kept as Release/nexus/options/looks-2.8.0.png.
"""

import math
import os
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import icons_and_colours as ic  # noqa: E402

OUT = os.path.join(ic.ROOT, "Release", "nexus", "looks.png")
U = ic.U
BONE = [0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F]
# The same ten icons as the first image, each at its own stage and fill.
ROW = [("hunger", 1, 0.30), ("thirst", 2, 0.45), ("cold", 0, 0.12), ("wetness", 3, 0.62), ("fatigue", 1, 0.28),
       ("vitality", 0, 0.08), ("disease", 4, 0.82), ("stress", 2, 0.45), ("dirt", 3, 0.62), ("encumbrance", 5, 1.0)]


def glyph(art, color):
    a = art.resize((42 * U, 42 * U), Image.LANCZOS)
    r, g, b, al = a.split()
    cr, cg, cb = ic.rgb(color)
    return Image.merge("RGBA", (r.point(lambda v: v * cr // 255), g.point(lambda v: v * cg // 255), b.point(lambda v: v * cb // 255), al))


def paint(t, mask, color, alpha=255):
    layer = Image.new("RGBA", t.size, ic.rgb(color) + (alpha,))
    t.alpha_composite(Image.composite(layer, Image.new("RGBA", t.size, (0, 0, 0, 0)), mask))


def tile(look, frames, art, color, f):
    c = 256
    if look == "ring":
        return ic.tile(frames["ring"], art, color, f)
    t = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    if look == "bar":
        t.alpha_composite(frames["bar"])
        m = Image.new("L", t.size, 0)
        ImageDraw.Draw(m).rectangle([c - 16.2 * U, c + 21.8 * U, c + (-16.2 + 32.4 * f) * U, c + 22.6 * U], fill=255)
        paint(t, m, color)
    elif look == "fill":
        t.alpha_composite(frames["ring"])
        # widget.as drawMeter "fill": inside the ring, from the bottom up to f, at 40%.
        r = 26.6 * U
        m = Image.new("L", t.size, 0)
        md = ImageDraw.Draw(m)
        md.ellipse([c - r, c - r, c + r, c + r], fill=255)
        level = c + r - 2 * r * f
        md.rectangle([0, 0, 512, level], fill=0)
        paint(t, m, color, int(255 * 0.40))
    t.alpha_composite(glyph(art, color), (c - 21 * U, c - 21 * U))
    return t


def main():
    with tempfile.TemporaryDirectory() as tmp:
        frames = {"ring": ic.export(tmp, "frame_ring"), "bar": ic.export(tmp, "frame_bar")}
        art = {n: ic.export(tmp, n) for n, _, _ in ROW}
    K = 2
    sheet = Image.new("RGBA", (1920 * K, 1080 * K), (0, 0, 0, 255))
    d = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(ic.GEORGIA, 46 * K)
    ICON, STEP = 124 * K, 150 * K
    X0 = 420 * K
    rows = [("Ring", "ring"), ("Bar", "bar"), ("Icon only", "icon"), ("Fill", "fill")]
    for ri, (label, look) in enumerate(rows):
        cy = (165 + ri * 250) * K
        d.text((300 * K, cy), label, font=font, fill="white", anchor="rm")
        for k, (n, stage, f) in enumerate(ROW):
            im = tile(look, frames, art[n], BONE[stage], f).resize((ICON, ICON), Image.LANCZOS)
            sheet.alpha_composite(im, (int(X0 + k * STEP - ICON / 2), int(cy - ICON / 2)))
    sheet.convert("RGB").resize((1920, 1080), Image.LANCZOS).save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
