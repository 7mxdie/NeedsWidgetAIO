"""The icon styles of 3.0 (Tom, 2026-11-23: "LOADS of choice", 50 in all with today's Ring).

One entry per style, in the order the menu lists them. Everything is our own drawing on the
64 x 64 canvas, origin top left, the icon's centre at (32, 32). Ported from the mock-ups in
art-options/ (styles_sheet.py, styles_batch4.py, styles_batch5.py) that Tom picked from.

    frame   SVG drawn behind the glyph: becomes icons/frame_<key>.swf through svg_frames.py.
            Only what never moves: the meter is drawn by widget.swf on top.
    meter   how the meter is drawn (widget.as setShape); kinds:
              ring   cx cy r w start (degrees clockwise from 12 o'clock)
              trace  d (path along which the meter runs from its first point) w, scale about the centre
              bar    x0 x1 y h, from "left" or "centre"
              steps  items (paths), w (0 = filled shapes), by "stage" or "share"; lit pieces in the stage colour
              level  d (the shape filled from the bottom like a liquid), alpha
              glyph  the glyph itself fills from the bottom (base glyph dimmed)
              none   nothing; tint_frame colours the frame instead
    glyph   "stage" (the glyph takes the stage colour) or a fixed "#rrggbb"
    size    the glyph's longer side on the canvas; cy its centre's height
    stages  the style's own six colours, best to worst
    own     always use the style's colours, whatever the player's colour scheme (dark-ink styles)
"""
import math

NORDIC = ["#f2efe6", "#e9dfc4", "#e0c27a", "#d98c4a", "#cc4a3c", "#ee3b2e"]


def poly(points):
    return "M" + " L".join(f"{x:.2f} {y:.2f}" for x, y in points) + " Z"


def diamond(cx, cy, s, fill):
    return f'<rect x="{cx - s:.1f}" y="{cy - s:.1f}" width="{2 * s}" height="{2 * s}" transform="rotate(45 {cx:.1f} {cy:.1f})" fill="{fill}"/>'


def cog(teeth=12, r_out=30, r_in=26.5):
    pts = []
    for k in range(teeth * 4):
        a = math.radians(-90 + 360 * k / (teeth * 4))
        r = r_out if k % 4 in (1, 2) else r_in
        pts.append((32 + r * math.cos(a), 32 + r * math.sin(a)))
    return poly(pts)


def star_ring(spikes=8, r_out=31, r_in=25):
    pts = []
    for k in range(spikes * 2):
        a = math.radians(-90 + 180 * k / spikes)
        r = r_out if k % 2 == 0 else r_in
        pts.append((32 + r * math.cos(a), 32 + r * math.sin(a)))
    return poly(pts)


def rough(n=28, r=27.5, wobble=1.6, seed=3):
    pts = []
    for k in range(n):
        a = math.radians(-90 + 360 * k / n)
        rr = r + wobble * math.sin(k * 2.7 + seed) * math.cos(k * 1.3)
        pts.append((32 + rr * math.cos(a), 32 + rr * math.sin(a)))
    return poly(pts)


def tri(cx, cy, s, fill, up=True):
    k = -1 if up else 1
    return f'<path d="M{cx} {cy + k * s} L{cx + s * .87:.2f} {cy - k * s / 2:.2f} L{cx - s * .87:.2f} {cy - k * s / 2:.2f} Z" fill="{fill}"/>'


def claws(colour):
    out = ""
    for a in (200, 230, 260):
        r0, r1 = 23, 31
        a0, a1 = math.radians(a), math.radians(a + 14)
        out += (f'<path d="M{32 + r0 * math.cos(a0):.1f} {32 + r0 * math.sin(a0):.1f} L{32 + r1 * math.cos(a1):.1f} {32 + r1 * math.sin(a1):.1f}" '
                f'stroke="{colour}" stroke-width="2.4" fill="none"/>')
    return out


def arc_d(cx, cy, r, a0, a1):
    """An open arc path, angles in degrees clockwise from 3 o'clock (SVG's own)."""
    x0, y0 = cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0))
    x1, y1 = cx + r * math.cos(math.radians(a1)), cy + r * math.sin(math.radians(a1))
    big = 1 if (a1 - a0) % 360 > 180 else 0
    return f"M{x0:.2f} {y0:.2f} A{r} {r} 0 {big} 1 {x1:.2f} {y1:.2f}"


# ---- shapes shared by frame and meter ----
DIAMOND = "M32 2 L62 32 L32 62 L2 32 Z"
DIAMOND_IN = "M32 7 L57 32 L32 57 L7 32 Z"
SHIELD = "M32 3 L57 9 L57 29 Q57 49 32 61 Q7 49 7 29 L7 9 Z"
SHIELD_IN = "M32 7.5 L53 12.5 L53 29 Q53 46 32 56.5 Q11 46 11 29 L11 12.5 Z"
HEX = "M14 8 L50 8 L61 32 L50 56 L14 56 L3 32 Z"
HEX_IN = "M16.5 12 L47.5 12 L56.5 32 L47.5 52 L16.5 52 L7.5 32 Z"
CLIPPED = poly([(12, 4), (52, 4), (60, 12), (60, 52), (52, 60), (12, 60), (4, 52), (4, 12)])
CLIPPED_IN = poly([(14, 8.5), (50, 8.5), (55.5, 14), (55.5, 50), (50, 55.5), (14, 55.5), (8.5, 50), (8.5, 14)])
LEAF = "M32 2 C50 10 62 22 62 32 C62 42 50 54 32 62 C14 54 2 42 2 32 C2 22 14 10 32 2 Z"
LEAF_IN = "M32 7.5 C46.5 14 56.5 24 56.5 32 C56.5 40 46.5 50 32 56.5 C17.5 50 7.5 40 7.5 32 C7.5 24 17.5 14 32 7.5 Z"
STONE = "M10 7 Q32 2 54 7 Q61 32 54 57 Q32 62 10 57 Q3 32 10 7 Z"
SNOWHEX = poly([(32 + 29 * math.cos(math.radians(-90 + 60 * k)), 32 + 29 * math.sin(math.radians(-90 + 60 * k))) for k in range(6)])
SNOWHEX_IN = poly([(32 + 24.5 * math.cos(math.radians(-90 + 60 * k)), 32 + 24.5 * math.sin(math.radians(-90 + 60 * k))) for k in range(6)])
ROUNDSQ = "M10 4 H54 Q60 4 60 10 V54 Q60 60 54 60 H10 Q4 60 4 54 V10 Q4 4 10 4 Z"
ARCH = "M32 2 Q52 10 58 30 V60 H6 V30 Q12 10 32 2 Z"
ARCH_IN = "M32 7 Q48.5 14 53.5 31 V55.5 H10.5 V31 Q15.5 14 32 7 Z"
GEM = poly([(32, 2), (52, 10), (61, 32), (52, 54), (32, 62), (12, 54), (3, 32), (12, 10)])
GEM_IN = poly([(32, 14), (44, 18), (50, 32), (44, 46), (32, 50), (20, 46), (14, 32), (20, 18)])
GEM_FACETS = ('<path d="M32 2 L32 14 M52 10 L44 18 M61 32 L50 32 M52 54 L44 46 M32 62 L32 50 M12 54 L20 46 M3 32 L14 32 M12 10 L20 18" '
              'fill="none" stroke="#c9a6ff" stroke-opacity=".35" stroke-width=".7"/>'
              f'<path d="{GEM_IN}" fill="none" stroke="#c9a6ff" stroke-opacity=".45" stroke-width=".7"/>')
BANNER = "M14 6 H50 V56 L32 46 L14 56 Z"
BANNER_IN = "M17.5 9.5 H46.5 V50 L32 42 L17.5 50 Z"
PIN = "M32 62 L15.2 40.3 A22 22 0 1 1 48.8 40.3 Z"
CRESCENT = "M8 12 Q8 62 32 61 Q56 62 56 12 Q52 50 32 51 Q12 50 8 12 Z"
CRESCENT_MID = "M8 12 Q10 58 32 56 Q54 58 56 12"
QUAT = "M18.17 18.17 A14 14 0 1 1 45.83 18.17 A14 14 0 1 1 45.83 45.83 A14 14 0 1 1 18.17 45.83 A14 14 0 1 1 18.17 18.17 Z"
CAPSULE = "M32 6 A16 16 0 0 1 48 22 V44 A16 16 0 0 1 16 44 V22 A16 16 0 0 1 32 6 Z"
ORC = poly([(8, 8), (26, 8), (26, 14), (38, 14), (38, 8), (56, 8), (56, 26), (50, 26), (50, 38), (56, 38), (56, 56), (38, 56),
            (38, 50), (26, 50), (26, 56), (8, 56), (8, 38), (14, 38), (14, 26), (8, 26)])
FLASK = "M27 6 H37 V22 A20 20 0 1 1 27 22 Z"
TRI = "M6 10 H58 L32 58 Z"
TRI_IN = "M12.5 14 H51.5 L32 50 Z"
STARS = [(10, 14), (54, 9), (59, 44), (32, 60), (5, 46)]
TSUBA_IN = "M32 9.5 A22.5 22.5 0 1 1 31.99 9.5 Z"
SHIELD_T = "M19 9 H45 V33 Q45 49 32 57 Q19 49 19 33 Z"
SHIELD_T_IN = "M22 12 H42 V33 Q42 46.5 32 53.5 Q22 46.5 22 33 Z"
WING = [(19, 13), (3, 8), (7, 16), (2, 19), (8, 24), (4, 28), (11, 32), (8, 36), (19, 34)]
SLAB = poly([(15, 2), (47, 4), (56, 24), (57, 62), (7, 62), (8, 24)])
SHELL = poly([(32, 1), (42, 10), (56, 6), (49, 20), (60, 27), (49, 33), (56, 46), (42, 46), (32, 63),
              (22, 46), (8, 46), (15, 33), (4, 27), (15, 20), (8, 6), (22, 10)])
SHELL_IN = "M32 8 Q47 20 46 33 Q44 45 32 54 Q20 45 18 33 Q17 20 32 8 Z"


def spiral_d():
    pts = []
    for n in range(0, 91):
        t = n / 90
        a = math.radians(-90 + 540 * t)
        r = 27 - 9 * t
        pts.append(f"{32 + r * math.cos(a):.2f} {33 + r * math.sin(a):.2f}")
    return "M" + " L".join(pts)


SPIRAL = spiral_d()
HAND = ('<ellipse cx="32" cy="42" rx="15" ry="15"/>'
        '<rect x="17.5" y="15" width="6" height="24" rx="3" transform="rotate(-14 20.5 39)"/>'
        '<rect x="25" y="8" width="6" height="28" rx="3" transform="rotate(-4 28 36)"/>'
        '<rect x="33" y="8" width="6" height="28" rx="3" transform="rotate(4 36 36)"/>'
        '<rect x="40.5" y="14" width="6" height="25" rx="3" transform="rotate(14 43.5 39)"/>'
        '<rect x="44" y="34" width="6" height="18" rx="3" transform="rotate(52 47 52)"/>')
RETICLE = ["M8 22 L8 8 L22 8", "M42 8 L56 8 L56 22", "M56 42 L56 56 L42 56", "M22 56 L8 56 L8 42"]
DAWN = []
for _n in range(9):
    _a = math.radians(180 + 180 * _n / 8)
    DAWN.append(f"M{32 + 23 * math.cos(_a):.2f} {38 + 23 * math.sin(_a):.2f} L{32 + 30 * math.cos(_a):.2f} {38 + 30 * math.sin(_a):.2f}")
GAUGE = [arc_d(32, 32, 26, 135 + n * 54 + 3.5, 135 + (n + 1) * 54 - 3.5) for n in range(5)]
PIPS = [poly([(x, y - 2.4 * 1.414), (x + 2.4 * 1.414, y), (x, y + 2.4 * 1.414), (x - 2.4 * 1.414, y)])
        for x, y in [(14, 54), (23, 56.5), (32, 57.5), (41, 56.5), (50, 54)]]
FANGS = ["M14 14 L20 14 L17 27 Z", "M44 14 L50 14 L47 27 Z", "M18 50 L24 50 L21 39 Z", "M40 50 L46 50 L43 39 Z", "M29 52 L35 52 L32 42 Z"]
CONST = [f"M{STARS[n][0]} {STARS[n][1]} L{STARS[(n + 1) % 5][0]} {STARS[(n + 1) % 5][1]}" for n in range(5)]


def star4(x, y):
    return (f'<path d="M{x} {y - 3} L{x + .9} {y - .9} L{x + 3} {y} L{x + .9} {y + .9} L{x} {y + 3} L{x - .9} {y + .9} '
            f'L{x - 3} {y} L{x - .9} {y - .9} Z" fill="#bfd4ff"/>')


def ring(r, w, cx=32, cy=32, start=0):
    return {"kind": "ring", "cx": cx, "cy": cy, "r": r, "w": w, "start": start}


def trace(d, w, scale=1.0):
    return {"kind": "trace", "d": d, "w": w, "scale": scale}


def bar(x0, x1, y, h, start="left"):
    return {"kind": "bar", "x0": x0, "x1": x1, "y": y, "h": h, "from": start}


def steps(items, w, by):
    return {"kind": "steps", "items": items, "w": w, "by": by}


def level(d, alpha, shapes=None):
    return {"kind": "level", "d": d, "alpha": alpha, "shapes": shapes}


GRADIENTS = {
    "aurora": ("linear", [(0, "#5fe3a1", 1), (.5, "#4fb6d9", 1), (1, "#a678e6", 1)]),
    "ember": ("radial", [(0, "#3a1606", .9), (.7, "#2a0e04", .85), (1, "#ff7a1a", .55)], .5, .5),
    "coin": ("radial", [(0, "#e9c96a", 1), (1, "#9a7428", 1)], .4, .35),
    "wood": ("radial", [(0, "#6b4526", 1), (1, "#3b2413", 1)], .45, .4),
}

S = []


def style(key, name, suits, note, frame, meter, glyph, size, stages, cy=32, own=False, tint_frame=False):
    S.append({"key": key, "name": name, "suits": suits, "note": note, "frame": frame, "meter": meter, "glyph": glyph,
              "size": size, "cy": cy, "stages": stages, "own": own, "tint_frame": tint_frame})


# ---- 2-16: the first sheets (Bone ring is today's Ring look, not a file) ----
style("plain", "Plain", "the base game's HUD", "No frame: a flat silhouette like the compass markers, a hairline ring meter.",
      "", ring(29, 1.4), "stage", 38, ["#ffffff", "#f2ede0", "#f0d98c", "#f0a860", "#e8684f", "#ff4a3a"])
style("mono", "Monochrome", "Norden UI", "Grey-white glyph, a thin pale circle on a charcoal disc. Colour only in the meter.",
      '<circle cx="32" cy="32" r="27" fill="#1c1d1f" fill-opacity=".78"/>'
      '<circle cx="32" cy="32" r="27" fill="none" stroke="#e6e6e6" stroke-opacity=".35" stroke-width="1"/>',
      ring(27, 2), "#dcdcdc", 30, ["#d8d8d8", "#c9c9c4", "#c8b88a", "#c79a6e", "#bd6b5e", "#d9534a"])
style("goldline", "Gold hairline", "Edge UI", "An olive-gold hairline circle with a gap at the top, gold glyph. Very light.",
      '<circle cx="32" cy="32" r="26" fill="#000000" fill-opacity=".28"/>'
      '<path d="M34.5 6.1 A26 26 0 1 1 29.5 6.1" fill="none" stroke="#b9a266" stroke-opacity=".75" stroke-width=".9"/>'
      '<line x1="32" y1="2.5" x2="32" y2="9" stroke="#c4ad6e" stroke-width="1.1"/>',
      ring(29.5, 1.6), "#c9b06f", 30, ["#c4ad6e", "#cdb571", "#d4a24f", "#cf7a3a", "#c2453a", "#e3362b"])
style("lightfantasy", "Light fantasy", "Dragonborn UI", "Cream glyph in a thin cream circle, a teal star above and a flourish below.",
      '<circle cx="32" cy="32" r="26" fill="#0a0c12" fill-opacity=".45"/>'
      '<circle cx="32" cy="32" r="26" fill="none" stroke="#efe6cf" stroke-opacity=".7" stroke-width=".8"/>'
      '<path d="M32 1.5 L33.2 4.8 L36.5 6 L33.2 7.2 L32 10.5 L30.8 7.2 L27.5 6 L30.8 4.8 Z" fill="#7fd6c4"/>'
      '<path d="M24 59 Q28 55.5 32 58.5 Q36 61.5 40 58" fill="none" stroke="#7fd6c4" stroke-width="1"/>',
      ring(29.5, 1.5), "#efe6cf", 32, ["#efe6cf", "#efe0b6", "#e7c77a", "#e39b5a", "#d9664f", "#f04a3c"])
style("nordic_diamond", "Nordic diamond", "Nordic UI", "A diamond like a shout meter: gold edge, teal inner line the meter runs along.",
      f'<path d="{DIAMOND}" fill="#0c0d0e" fill-opacity=".8" stroke="#b49a63" stroke-width="1.2"/>'
      f'<path d="{DIAMOND_IN}" fill="none" stroke="#6fb9ad" stroke-opacity=".45" stroke-width=".8"/>',
      trace(DIAMOND_IN, 2.2), "#f2efe6", 26, NORDIC)
style("nordic_shield", "Nordic shield", "Nordic UI", "A heraldic shield with a gold double line; the meter runs down the inner line.",
      f'<path d="{SHIELD}" fill="#0c0d0e" fill-opacity=".8" stroke="#b49a63" stroke-width="1.3"/>'
      f'<path d="{SHIELD_IN}" fill="none" stroke="#b49a63" stroke-opacity=".45" stroke-width=".7"/>',
      trace(SHIELD_IN, 2.2), "#f2efe6", 28, NORDIC)
style("nordic_hex", "Nordic hexagon", "Nordic UI", "A long hexagon like a menu button, gold edge, notches at the points.",
      f'<path d="{HEX}" fill="#0c0d0e" fill-opacity=".8" stroke="#b49a63" stroke-width="1.2"/>'
      f'<path d="{HEX_IN}" fill="none" stroke="#b49a63" stroke-opacity=".45" stroke-width=".7"/>'
      + diamond(3, 32, 1.8, "#cbb487") + diamond(61, 32, 1.8, "#cbb487"),
      trace(HEX_IN, 2.2), "#f2efe6", 30, NORDIC)
style("dwemer", "Dwemer", "bronze and cogs", "A bronze cog with rivets; the glyph in pale brass.",
      f'<path d="{cog()}" fill="#2a1d10" fill-opacity=".85" stroke="#b07a3c" stroke-width="1.2"/>'
      '<circle cx="32" cy="32" r="22" fill="none" stroke="#b07a3c" stroke-opacity=".55" stroke-width=".8"/>'
      + "".join(f'<circle cx="{32 + 24.3 * math.cos(math.radians(a)):.1f}" cy="{32 + 24.3 * math.sin(math.radians(a)):.1f}" r="1.1" fill="#d5a35a"/>' for a in (45, 135, 225, 315)),
      ring(22, 2.2), "#ead3a3", 26, ["#e9d3a6", "#e6c98a", "#e0aa55", "#d77d3a", "#c64a33", "#ee3b2e"])
style("daedric", "Daedric", "dark and red", "A black eight-spiked star edged in ember red; bone glyph.",
      f'<path d="{star_ring()}" fill="#0b0606" fill-opacity=".88" stroke="#8f1f1f" stroke-width="1.3"/>'
      '<circle cx="32" cy="32" r="21" fill="none" stroke="#b8352a" stroke-opacity=".5" stroke-width=".8"/>',
      ring(21, 2.2), "#ecdcd4", 25, ["#eadcd6", "#e6c3b4", "#e29a72", "#dc6b45", "#d23a2c", "#ff2a1c"])
style("elven", "Elven", "slender gold", "A pointed almond in pale gold; the meter runs along its inner line.",
      f'<path d="{LEAF}" fill="#101208" fill-opacity=".7" stroke="#d8c27a" stroke-width="1"/>'
      f'<path d="{LEAF_IN}" fill="none" stroke="#d8c27a" stroke-opacity=".4" stroke-width=".6"/>',
      trace(LEAF_IN, 2), "#f1e6c6", 30, ["#f3ead0", "#eedfae", "#e5c36c", "#dc9a4f", "#cf5c40", "#f24433"])
style("imperial", "Imperial", "red and gold", "A square with cut corners, deep Imperial red, gold border.",
      f'<path d="{CLIPPED}" fill="#4a1010" fill-opacity=".88" stroke="#c9a85a" stroke-width="1.3"/>'
      f'<path d="{CLIPPED_IN}" fill="none" stroke="#c9a85a" stroke-opacity=".45" stroke-width=".7"/>',
      trace(CLIPPED_IN, 2.2), "#f4efe2", 30, ["#f4efe2", "#efdcb0", "#e9c060", "#e38b3e", "#d94a33", "#ff3a2a"])
style("stormcloak", "Stormcloak", "blue, rough-hewn", "A rough slate-blue disc like hewn stone, pale glyph.",
      f'<path d="{rough()}" fill="#243447" fill-opacity=".85" stroke="#9fb3c4" stroke-width="1.1"/>',
      ring(23.5, 2.2), "#eef3f6", 30, ["#e9eef2", "#d8e0e6", "#d9c27e", "#d8904e", "#cc4d3c", "#f03c2e"])
style("parchment", "Parchment", "the map and books", "Aged paper with an ink ring and an ink glyph. Reads best over dark scenes.",
      '<circle cx="32" cy="32" r="28" fill="#d9c8a0" fill-opacity=".95" stroke="#5a4428" stroke-width="1.4"/>'
      '<circle cx="32" cy="32" r="24.5" fill="none" stroke="#5a4428" stroke-opacity=".5" stroke-width=".7"/>',
      ring(28, 2.6), "stage", 30, ["#3a2c1c", "#4a3520", "#8a5a1c", "#a5501f", "#a82a1c", "#c41a10"], own=True)
style("runestone", "Runestone", "carved stone", "A grey standing-stone slab, the glyph pale as if carved in.",
      f'<path d="{STONE}" fill="#4b4d4a" fill-opacity=".9" stroke="#8d8f89" stroke-width="1.1"/>',
      trace(STONE, 2.4), "#e3e3dd", 30, ["#e2e2dc", "#d6d5cc", "#d2bd82", "#cc9257", "#c05845", "#e5463a"])
style("frost", "Frost", "icy blue", "An ice-blue hexagon, frosty white glyph.",
      f'<path d="{SNOWHEX}" fill="#0b1d2a" fill-opacity=".75" stroke="#8fd0ef" stroke-width="1.1"/>'
      f'<path d="{SNOWHEX_IN}" fill="none" stroke="#8fd0ef" stroke-opacity=".4" stroke-width=".7"/>',
      trace(SNOWHEX_IN, 2.2), "#e6f6ff", 28, ["#e6f6ff", "#cdeaf8", "#e7d38a", "#e59a5a", "#d9574a", "#ff4436"])

# ---- 17-28: third batch ----
style("flat", "Flat tile", "Untarnished UI", "A translucent grey rounded square, flat white glyph, a flat bar meter.",
      f'<path d="{ROUNDSQ}" fill="#3b3c3e" fill-opacity=".72" stroke="#ffffff" stroke-opacity=".18" stroke-width=".8"/>',
      bar(10, 54, 53, 2.6), "#f4f4f4", 30, ["#f2f2f2", "#e2e2e2", "#e6cf86", "#e5a05c", "#de6a55", "#ff5446"])
style("signal", "Signal", "bold, colour first", "The whole tile takes the need's colour and the glyph is dark.",
      '<circle cx="32" cy="32" r="28" fill="#ffffff" fill-opacity=".92" stroke="#000000" stroke-opacity=".5" stroke-width="1.2"/>',
      {"kind": "none"}, "#17130e", 32, ["#d9d4c4", "#e0cf95", "#e6b54f", "#e3863e", "#d9483a", "#ff2e22"], tint_frame=True)
style("underline", "Underline", "the barest HUD", "No frame: the glyph with a short bar under it that is the meter.",
      '<rect x="14" y="54" width="36" height="2.6" rx="1.3" fill="#000000" fill-opacity=".45"/>',
      bar(14, 50, 54, 2.6), "stage", 34, ["#ffffff", "#f2ede0", "#f0d98c", "#f0a860", "#e8684f", "#ff4a3a"])
style("volkihar", "Volkihar", "vampire, gothic", "A gothic arch in black-violet with a crimson edge; the meter climbs the arch.",
      f'<path d="{ARCH}" fill="#140812" fill-opacity=".88" stroke="#8a1a2c" stroke-width="1.3"/>'
      f'<path d="{ARCH_IN}" fill="none" stroke="#8a1a2c" stroke-opacity=".5" stroke-width=".7"/>',
      trace(ARCH_IN, 2.2), "#efe3ea", 28, ["#efe3ea", "#e6c7d3", "#d9988f", "#cf6a5a", "#c2303a", "#ff1f3a"])
style("nightingale", "Nightingale", "shadowy green-grey", "A dark green-grey disc with three small triangles round it; dim silver glyph.",
      '<circle cx="32" cy="32" r="26" fill="#121813" fill-opacity=".85" stroke="#5d6e60" stroke-width="1.1"/>'
      + tri(32, 4, 3.4, "#7f9583") + tri(32 + 27 * math.cos(math.radians(30)), 32 + 27 * math.sin(math.radians(30)), 3.4, "#7f9583", False)
      + tri(32 - 27 * math.cos(math.radians(30)), 32 + 27 * math.sin(math.radians(30)), 3.4, "#7f9583", False),
      ring(26, 2.2), "#d3dad3", 29, ["#cfd6cf", "#bcc7bd", "#c8b97a", "#c48f55", "#b85a45", "#e2453a"])
style("arcane", "Arcane", "mages", "A violet disc inside a dashed rune circle; pale blue glyph.",
      '<circle cx="32" cy="32" r="25" fill="#160f2a" fill-opacity=".82"/>'
      '<circle cx="32" cy="32" r="29" fill="none" stroke="#9d86ff" stroke-width="1.2" stroke-dasharray="2.2 1.6 6 1.6"/>'
      '<circle cx="32" cy="32" r="25" fill="none" stroke="#9d86ff" stroke-opacity=".45" stroke-width=".7"/>',
      ring(25, 2.2), "#e3ecff", 28, ["#e3ecff", "#c9d6ff", "#d9c27e", "#d8904e", "#cc4d5c", "#ff3c6e"])
style("soulgem", "Soul Gem", "enchanting, purple", "A faceted violet gem; the meter runs round the inner facet.",
      f'<path d="{GEM}" fill="#24123d" fill-opacity=".85" stroke="#b48cff" stroke-width="1.1"/>' + GEM_FACETS,
      trace(GEM_IN, 2), "#f1e9ff", 24, ["#f1e9ff", "#dccbff", "#e2c27a", "#dc914e", "#cf4d5a", "#ff3a5e"])
style("dragon", "Dragon", "claw marks", "A dark slate disc with three ember claw slashes; bone glyph.",
      '<circle cx="32" cy="32" r="27" fill="#16181b" fill-opacity=".88" stroke="#6b6f74" stroke-width="1.1"/>' + claws("#e2733a"),
      ring(27, 2.2, start=90), "#ece4d4", 28, ["#ece4d4", "#e6d3b0", "#e2b065", "#de7d40", "#d1442e", "#ff3320"])
style("aurora", "Aurora", "northern lights", "A dark disc with a ring shading green to blue to violet; white glyph.",
      '<circle cx="32" cy="32" r="27" fill="#0a0f18" fill-opacity=".82"/>'
      '<circle cx="32" cy="32" r="28" fill="none" stroke="url(#aurora)" stroke-width="2"/>',
      ring(23.5, 2), "#ffffff", 28, ["#ffffff", "#e6f5ef", "#e6d38a", "#e59a5a", "#d9574a", "#ff4436"])
style("ember", "Ember", "firelight", "No hard frame: a dark centre glowing orange at its edge, like a coal.",
      '<circle cx="32" cy="32" r="29" fill="url(#ember)"/>',
      ring(29, 1.8), "#fff1d8", 30, ["#fff1d8", "#ffe2b0", "#ffc067", "#ff9440", "#ff5a2a", "#ff2a14"])
style("hearthfire", "Hearthfire", "homesteads, wood", "A square wooden plank with grain and two nails; light glyph.",
      '<rect x="5" y="6" width="54" height="52" rx="3" fill="url(#wood)" stroke="#2a180b" stroke-width="1.4"/>'
      '<path d="M7 18 Q32 15 57 19 M7 31 Q30 34 57 30 M7 45 Q34 42 57 46" fill="none" stroke="#2a180b" stroke-opacity=".35" stroke-width=".7"/>'
      '<circle cx="10.5" cy="11.5" r="1.3" fill="#a88a64"/><circle cx="53.5" cy="11.5" r="1.3" fill="#a88a64"/>',
      bar(10, 54, 52.5, 2.8), "#f3e6cc", 30, ["#f3e6cc", "#ecd5a8", "#e6b562", "#dc8540", "#cc4a32", "#f03a26"])
style("septim", "Septim", "a gold coin", "A gold coin with a dotted rim; the glyph stamped dark into it.",
      '<circle cx="32" cy="32" r="28" fill="url(#coin)" stroke="#6e5218" stroke-width="1.3"/>'
      + "".join(f'<circle cx="{32 + 25 * math.cos(math.radians(a)):.1f}" cy="{32 + 25 * math.sin(math.radians(a)):.1f}" r=".8" fill="#6e5218"/>' for a in range(0, 360, 15)),
      ring(29.5, 2.2), "stage", 28, ["#2b1f08", "#3a2a0a", "#6e4a10", "#8a3e14", "#8f1f12", "#b0140c"], own=True)

# ---- 29-44: Fable 5.2's ideas ----
style("hollow", "Hollow", "the glyph is the meter", "No frame: the glyph is drawn dim and fills with colour from the bottom.",
      "", {"kind": "glyph"}, "#5a5a5a", 40, NORDIC)
style("banner", "Hold banner", "a hold's pennant", "A swallowtail banner on a rod; the meter runs along its edge.",
      f'<path d="{BANNER}" fill="#2b3440" fill-opacity=".88" stroke="#b9a46a" stroke-width="1.2"/>'
      '<line x1="10" y1="6" x2="54" y2="6" stroke="#b9a46a" stroke-width="2"/>',
      trace(BANNER_IN, 2.2), "#f2ebd9", 24, NORDIC, cy=27)
style("reticle", "Reticle", "open corners", "Four corner brackets; they light one by one as it gets worse.",
      "".join(f'<path d="{d}" fill="none" stroke="#000000" stroke-opacity=".6" stroke-width="5"/>'
              f'<path d="{d}" fill="none" stroke="#d8d8d8" stroke-width="3"/>' for d in RETICLE),
      steps(RETICLE, 3, "stage"), "#ffffff", 30, NORDIC)
style("marker", "Map marker", "a map pin", "A teardrop pin; the meter rings its head.",
      f'<path d="{PIN}" fill="#1e1e1e" fill-opacity=".88" stroke="#cfc8b4" stroke-width="1.2"/>',
      ring(19, 2.2, cy=26), "#ffffff", 24, NORDIC, cy=26)
style("forsworn", "Forsworn", "bone crescent", "A bone crescent cradling the glyph; the meter runs along it tip to tip.",
      f'<path d="{CRESCENT}" fill="#d9cdb0" stroke="#3a2e22" stroke-width="1.3"/>',
      trace(CRESCENT_MID, 2.6), "#ffffff", 28, NORDIC, cy=28)
style("compass", "Compass", "compass rails", "Two rails like a slice of the compass bar; the bottom one fills from the centre out.",
      "".join(f'<path d="M6 {y} H58 M6 {y - 3} V{y + 3} M58 {y - 3} V{y + 3}" fill="none" stroke="#000000" stroke-opacity=".55" stroke-width="3"/>'
              f'<path d="M6 {y} H58 M6 {y - 3} V{y + 3} M58 {y - 3} V{y + 3}" fill="none" stroke="#c9c3b0" stroke-width="1.2"/>' for y in (10, 54)),
      bar(6, 58, 52.5, 3, "centre"), "#ffffff", 32, NORDIC)
style("dawnguard", "Dawnguard", "half sunburst", "Nine rays fanned above the glyph; they light left to right.",
      "".join(f'<path d="{d}" fill="none" stroke="#000000" stroke-opacity=".5" stroke-width="4.2"/>'
              f'<path d="{d}" fill="none" stroke="#e8b84a" stroke-width="2.4"/>' for d in DAWN),
      steps(DAWN, 2.4, "share"), "#fff6dc", 28, NORDIC, cy=38)
style("divines", "Divines", "quatrefoil window", "A four-lobed window; the meter runs along the inner outline.",
      f'<path d="{QUAT}" fill="#1f2a44" fill-opacity=".9" stroke="#d4b55a" stroke-width="1.2"/>'
      f'<g transform="translate(32 32) scale(.82) translate(-32 -32)"><path d="{QUAT}" fill="none" stroke="#d4b55a" stroke-opacity=".4" stroke-width=".8"/></g>',
      trace(QUAT, 2.6, .82), "#f4f0e0", 28, NORDIC)
style("lantern", "Lantern", "a liquid level", "A tall pill with a hanging loop; the stage colour fills it like a liquid.",
      '<circle cx="32" cy="3.5" r="2.6" fill="none" stroke="#6b5a3a" stroke-width="1.2"/>'
      f'<path d="{CAPSULE}" fill="#14110d" fill-opacity=".9" stroke="#6b5a3a" stroke-width="1.3"/>',
      level(CAPSULE, 55), "#ffffff", 26, NORDIC, cy=33)
style("gauge", "Gauge", "chunky segments", "No frame: a thick arc in five blocks; one lights per stage.",
      "".join(f'<path d="{d}" fill="none" stroke="#000000" stroke-opacity=".5" stroke-width="9"/>'
              f'<path d="{d}" fill="none" stroke="#3a3a3a" stroke-width="6.5"/>' for d in GAUGE),
      steps(GAUGE, 6.5, "stage"), "#ffffff", 30, NORDIC)
style("orcish", "Orcish", "notched iron", "A square with a notch bitten out of each side; the meter runs along it.",
      f'<path d="{ORC}" fill="#2e3526" fill-opacity=".9" stroke="#7c8a5a" stroke-width="1.3"/>',
      trace(ORC, 2.4), "#e8e6d8", 28, NORDIC)
style("scroll", "Elder Scroll", "a scroll", "Paper between two wooden rollers; an ink glyph and a bar meter.",
      '<rect x="14" y="13" width="36" height="38" fill="#e6d9b5" stroke="#8a6a2e" stroke-width="1"/>'
      '<rect x="8" y="9" width="8" height="46" rx="3.5" fill="#7a5a26" stroke="#3e2c10" stroke-width="1"/>'
      '<rect x="48" y="9" width="8" height="46" rx="3.5" fill="#7a5a26" stroke="#3e2c10" stroke-width="1"/>'
      '<circle cx="12" cy="6.5" r="2.6" fill="#b08a3e"/><circle cx="12" cy="57.5" r="2.6" fill="#b08a3e"/>'
      '<circle cx="52" cy="6.5" r="2.6" fill="#b08a3e"/><circle cx="52" cy="57.5" r="2.6" fill="#b08a3e"/>',
      bar(17, 47, 46.5, 2.4), "#2a2118", 26, NORDIC, cy=30)
style("flask", "Alchemy flask", "a potion", "A round flask with a neck; the liquid is the meter.",
      '<rect x="24" y="3" width="16" height="4" rx="1.5" fill="#7fb8a8"/>'
      f'<path d="{FLASK}" fill="#10201c" fill-opacity=".9" stroke="#7fb8a8" stroke-width="1.2"/>',
      level(FLASK, 55), "#ffffff", 24, NORDIC, cy=41)
style("pips", "Pips", "five dots", "No frame: five diamond pips under the glyph, one per stage.",
      "".join(f'<path d="{poly([(x, y - 4.8), (x + 4.8, y), (x, y + 4.8), (x - 4.8, y)])}" fill="#000000" fill-opacity=".55"/>'
              f'<path d="{d}" fill="#4a4a4a"/>' for (x, y), d in zip([(14, 54), (23, 56.5), (32, 57.5), (41, 56.5), (50, 54)], PIPS)),
      steps(PIPS, 0, "stage"), "#ffffff", 34, NORDIC, cy=28)
style("shadowmark", "Shadowmark", "thieves' triangle", "A down-pointing triangle with a scratch; the meter runs along its edge.",
      f'<path d="{TRI}" fill="#1a1a1a" fill-opacity=".9" stroke="#9a9a8a" stroke-width="1.2"/>'
      '<path d="M13 16 L28 44" fill="none" stroke="#9a9a8a" stroke-opacity=".4" stroke-width=".8"/>',
      trace(TRI_IN, 2.2), "#dddddd", 21, NORDIC, cy=26)
style("constellation", "Constellation", "stars", "Five stars round the glyph, joined by lines that light in turn.",
      "".join(f'<path d="{d}" fill="none" stroke="#bfd4ff" stroke-opacity=".35" stroke-width=".8"/>' for d in CONST)
      + "".join(star4(x, y) for x, y in STARS),
      steps(CONST, 1.6, "share"), "#ffffff", 32, NORDIC)

# ---- 45-50: guilds and peoples ----
style("brotherhood", "Dark Brotherhood", "black hand", "A black hand print; the stage colour rises inside it.",
      f'<g fill="#000000" fill-opacity=".6" transform="translate(32 33) scale(1.08) translate(-32 -33)">{HAND}</g>'
      f'<g fill="#1c0d0d">{HAND}</g>',
      level("", 60, HAND), "#ffffff", 22, NORDIC, cy=42)
style("companions", "Companions", "wolf fangs", "Jaws round the picture; one more fang lights per stage.",
      '<circle cx="32" cy="32" r="27" fill="#000000" fill-opacity=".45"/>'
      '<path d="M6 18 Q32 4 58 18 L58 13 Q32 -1 6 13 Z" fill="#5a4630" stroke="#2a1d10" stroke-width="1"/>'
      '<path d="M8 47 Q32 62 56 47 L56 52 Q32 67 8 52 Z" fill="#5a4630" stroke="#2a1d10" stroke-width="1"/>'
      + "".join(f'<path d="{d}" fill="#e8e0cc" stroke="#000000" stroke-opacity=".7" stroke-width="1"/>' for d in FANGS),
      steps(FANGS, 0, "stage"), "#ffffff", 26, NORDIC)
style("blades", "Blades", "sword guard", "A round sword guard with cut-outs; the meter runs round the inner ring.",
      '<path d="M32 3 A29 29 0 1 1 31.99 3 Z M11 26 A3.5 6.5 0 1 0 11.01 26 Z M53 26 A3.5 6.5 0 1 0 53.01 26 Z" fill="#24211d" '
      'fill-opacity=".95" stroke="#8a7a5a" stroke-width="1.3"/>'
      '<circle cx="32" cy="32" r="18.5" fill="none" stroke="#8a7a5a" stroke-opacity=".5" stroke-width=".8"/>',
      trace(TSUBA_IN, 2.6), "#f0ebe0", 26, NORDIC)
style("thalmor", "Thalmor", "eagle crest", "A black shield between gold wings; the meter runs along the shield.",
      f'<path d="{poly(WING)}" fill="#c9a24a" stroke="#4a3510" stroke-width="1"/>'
      f'<path d="{poly([(64 - x, y) for x, y in WING])}" fill="#c9a24a" stroke="#4a3510" stroke-width="1"/>'
      f'<path d="{SHIELD_T}" fill="#121212" stroke="#c9a24a" stroke-width="1.4"/>',
      trace(SHIELD_T_IN, 2.2), "#f2ead2", 20, NORDIC, cy=31)
style("draugr", "Draugr", "barrow stone", "A standing stone; the carved spiral is the meter.",
      f'<path d="{SLAB}" fill="#4a4d48" stroke="#22241f" stroke-width="1.3"/>'
      f'<path d="{SPIRAL}" fill="none" stroke="#2a2c28" stroke-width="2.4"/>',
      trace(SPIRAL, 2.2), "#dfe6e8", 18, NORDIC, cy=33)
style("falmer", "Falmer", "chitin plate", "A barbed chitin plate; the meter runs along the inner ridge.",
      f'<path d="{SHELL}" fill="#1f2b29" fill-opacity=".95" stroke="#9fb3a4" stroke-width="1.2"/>'
      + "".join(f'<path d="M{24 - 2 * n} {16 + 8 * n} Q32 {12 + 8 * n} {40 + 2 * n} {16 + 8 * n}" fill="none" stroke="#9fb3a4" stroke-opacity=".3" stroke-width="1"/>' for n in range(4)),
      trace(SHELL_IN, 2.2), "#e6efe8", 20, NORDIC, cy=31)

STYLES = S
assert len({s["key"] for s in S}) == len(S) == 49, len(S)
