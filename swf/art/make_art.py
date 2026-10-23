"""Draw the vanilla-look icon art and write it as SVG.

    python make_art.py

Each glyph is built from simple shapes (circles, capsules, polygons) combined as
signed distance fields: union is min, a cut is max(a, -b). The field is sampled
on a fine grid and traced into outlines (marching squares), so every glyph comes
out as clean outlines with no overlapping pieces. That matters because Scaleform
fades a clip piece by piece: overlapping white shapes would show brighter seams
when the icon's opacity is turned down.

Style: solid silhouettes with sharp, hand-cut edges and a carved detail line,
like Skyrim's own map and compass markers rather than rounded web icons, with a
soft dark edge so they read over snow and sky alike.

Output: drawn/<name>.svg, 64 x 64 units, origin top left, y down:
the halo bands (black, part-transparent) first, then the glyph in white.
build_icons.py turns them into Interface/NeedsWidget/icons/<name>.swf.
"""

import math
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
CANVAS = 64.0
N = 768                      # grid cells per side: 12 per unit
TOLERANCE = 0.02             # outline simplification, in units
MIN_AREA = 1.0               # outlines smaller than this (square units) are dropped

# The soft dark edge round every glyph. Each band: (from, to, opacity), measured
# outward from the glyph's edge in units. The bands don't overlap, so the glow
# fades evenly when the icon's opacity is turned down.
HALO = [(1.6, 2.7, 0.14), (0.8, 1.6, 0.30), (0.0, 0.8, 0.52)]

_c = np.arange(N + 1) * CANVAS / N
X, Y = np.meshgrid(_c, _c)   # X[j, i] = x of column i, Y[j, i] = y of row j


# --- shapes: signed distance, negative inside ---------------------------------

def circle(cx, cy, r):
    return np.hypot(X - cx, Y - cy) - r


def segment(ax, ay, bx, by, r):
    px, py = X - ax, Y - ay
    dx, dy = bx - ax, by - ay
    h = np.clip((px * dx + py * dy) / (dx * dx + dy * dy), 0.0, 1.0)
    return np.hypot(px - dx * h, py - dy * h) - r


def tapered(points, r0, r1):
    # A polyline whose thickness runs from r0 at the first segment to r1 at the last.
    n = len(points) - 1
    return union(*[segment(a[0], a[1], b[0], b[1], r0 + (r1 - r0) * k / max(n - 1, 1))
                   for k, (a, b) in enumerate(zip(points, points[1:]))])


def ellipse(cx, cy, rx, ry, degrees):
    # Exact zero contour, approximate distance elsewhere: enough for tracing.
    t = math.radians(degrees)
    u = (X - cx) * math.cos(t) + (Y - cy) * math.sin(t)
    v = -(X - cx) * math.sin(t) + (Y - cy) * math.cos(t)
    return (np.hypot(u / rx, v / ry) - 1.0) * min(rx, ry)


def box(cx, cy, hw, hh, r, degrees=0.0):
    # Rounded rectangle, half width hw, half height hh, corner radius r, turned.
    t = math.radians(degrees)
    u = (X - cx) * math.cos(t) + (Y - cy) * math.sin(t)
    v = -(X - cx) * math.sin(t) + (Y - cy) * math.cos(t)
    qx, qy = np.abs(u) - (hw - r), np.abs(v) - (hh - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def polygon(points):
    # Signed distance to a simple polygon (after Inigo Quilez's sdPolygon).
    d = (X - points[0][0]) ** 2 + (Y - points[0][1]) ** 2
    s = np.ones_like(X)
    j = len(points) - 1
    for i in range(len(points)):
        (xi, yi), (xj, yj) = points[i], points[j]
        ex, ey = xj - xi, yj - yi
        wx, wy = X - xi, Y - yi
        h = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0.0, 1.0)
        d = np.minimum(d, (wx - ex * h) ** 2 + (wy - ey * h) ** 2)
        c1, c2, c3 = Y >= yi, Y < yj, ex * wy > ey * wx
        s = np.where((c1 & c2 & c3) | (~c1 & ~c2 & ~c3), -s, s)
        j = i
    return s * np.sqrt(d)


def halfplane(px, py, nx, ny):
    # Signed distance to the line through (px, py), positive on the side n points to.
    return (X - px) * nx + (Y - py) * ny


def spear(cx, cy, degrees, start, length, width):
    # A narrow diamond pointing along `degrees` (0 = right, 90 = up), widest a
    # third of the way along.
    a = math.radians(degrees)
    ux, uy = math.cos(a), -math.sin(a)
    vx, vy = -uy, ux
    m = start + length * 0.35
    return polygon([
        (cx + ux * start, cy + uy * start),
        (cx + ux * m + vx * width, cy + uy * m + vy * width),
        (cx + ux * (start + length), cy + uy * (start + length)),
        (cx + ux * m - vx * width, cy + uy * m - vy * width),
    ])


def star(cx, cy, outer, inner, points=4, degrees=90.0):
    pts = []
    for k in range(points * 2):
        a = math.radians(degrees) + math.pi * k / points
        r = outer if k % 2 == 0 else inner
        pts.append((cx + math.cos(a) * r, cy - math.sin(a) * r))
    return polygon(pts)


def hexagon(cx, cy, r, degrees=90.0):
    return polygon([(cx + math.cos(math.radians(degrees + 60 * k)) * r,
                     cy - math.sin(math.radians(degrees + 60 * k)) * r) for k in range(6)])


def ring(cx, cy, r, width):
    return cut(circle(cx, cy, r), circle(cx, cy, r - width))


def union(*fields):
    return np.minimum.reduce(fields)


def smooth_union(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b * (1 - h) + a * h - k * h * (1 - h)


def cut(a, b):
    return np.maximum(a, -b)


def tangent_point(px, py, cx, cy, r, side):
    # Where a line from (px, py) touches the circle (cx, cy, r); side picks which.
    dx, dy = cx - px, cy - py
    dist = math.hypot(dx, dy)
    a = math.atan2(dy, dx) + side * math.asin(r / dist)
    length = math.sqrt(dist * dist - r * r)
    return px + math.cos(a) * length, py + math.sin(a) * length


# --- the glyphs ---------------------------------------------------------------

def hunger():
    # A roast leg: the meat up and to the right, ending in a clean cut, then the
    # bone. A highlight is carved into the top of the meat.
    away = (-0.7071, 0.7071)                 # along the bone, away from the meat
    side = halfplane(28.0, 35.0, *away)
    meat = smooth_union(ellipse(38.5, 24.5, 17.0, 13.5, -45), segment(34, 29, 26, 37, 5.3), 5.0)
    meat = np.maximum(meat, side + 0.9)
    end_x, end_y = 15.0, 49.0
    bone = union(segment(30, 33, end_x, end_y, 3.1),
                 circle(end_x + 2.97, end_y + 2.97, 4.5),
                 circle(end_x - 2.97, end_y - 2.97, 4.5))
    bone = np.maximum(bone, 0.9 - side)
    shine = tapered([(37.5, 14.4), (42.0, 13.0), (46.0, 13.7)], 1.0, 0.7)
    return union(cut(meat, shine), bone)


def cold():
    # An ice crystal: six spear-tipped arms, each with two pairs of barbs, round
    # a hollow hexagon.
    cx, cy = 32.0, 32.0
    parts = [hexagon(cx, cy, 6.0)]
    for k in range(6):
        a = 90 + 60 * k
        ux, uy = math.cos(math.radians(a)), -math.sin(math.radians(a))
        parts.append(spear(cx, cy, a, 0.0, 26.5, 2.8))
        for at, length, width in ((10.0, 9.5, 1.75), (17.0, 6.8, 1.45)):
            bx, by = cx + ux * at, cy + uy * at
            for turn in (-52, 52):
                parts.append(spear(bx, by, a + turn, -1.5, length + 1.5, width))
    return cut(union(*parts), hexagon(cx, cy, 2.7))


def fatigue():
    # A crescent moon with two stars in the dark beside it.
    moon = cut(circle(29.5, 35.0, 21.0), circle(40.0, 26.0, 17.5))
    return union(moon, star(46.5, 11.5, 7.0, 1.6), star(55.0, 25.5, 4.3, 1.05))


def stress():
    # A heart with a jagged crack running down from the top, stopping short of
    # the point so the heart stays in one piece.
    left, right, r = (22.5, 24.5), (41.5, 24.5), 12.6
    tip = (32.0, 55.0)
    tl = tangent_point(tip[0], tip[1], left[0], left[1], r, -1)    # outer side of each lobe
    tr = tangent_point(tip[0], tip[1], right[0], right[1], r, 1)
    heart = union(circle(left[0], left[1], r), circle(right[0], right[1], r),
                  polygon([tl, tip, tr, right, left]))
    crack = tapered([(32.0, 9.0), (32.0, 17.0), (28.2, 25.0), (34.8, 32.0), (28.8, 40.0), (32.6, 47.0)], 1.7, 0.75)
    return cut(heart, crack)


def dirt():
    # Dirt and Blood: a cut block of soap with bubbles. Players know this one
    # from the iWant Dirt and Blood widget; the drawing is our own.
    bar = box(31.0, 45.0, 20.0, 10.0, 2.5, -8)
    face = box(31.0, 43.0, 15.0, 5.5, 1.5, -8)
    bubbles = union(ring(19.5, 22.5, 6.0, 1.9), ring(34.0, 14.5, 4.6, 1.7), circle(46.0, 24.0, 2.8), circle(28.0, 29.0, 2.0))
    return union(cut(bar, cut(face + 0.9, face)), bubbles)


def dirt_splash():
    # Alternative for Dirt and Blood: a splash of muck. Reads too much like a germ.
    core = smooth_union(circle(31.0, 31.0, 11.0), circle(37.5, 25.0, 7.0), 4.0)
    core = smooth_union(core, circle(24.0, 37.0, 6.5), 4.0)
    rays = [smooth_union(segment(31.0, 31.0, ex, ey, 1.5), circle(ex, ey, r_end), 2.2)
            for ex, ey, r_end in ((11.5, 19.0, 3.0), (51.5, 13.5, 2.6), (53.5, 38.5, 3.1), (12.5, 46.5, 2.4))]
    drip = union(segment(35.0, 39.0, 35.0, 51.0, 2.3), circle(35.0, 52.5, 3.2))
    splash = smooth_union(smooth_union(core, union(*rays), 2.5), drip, 2.5)
    flecks = union(circle(44.0, 7.5, 2.1), circle(6.5, 32.0, 1.8), circle(21.0, 55.0, 1.9), circle(57.5, 27.0, 1.6))
    return union(splash, flecks)


def encumbrance():
    # A forged iron weight: chamfered body, ring handle, a groove cut near the
    # base. SkyUI's inventory uses a weight for carry weight, so players read it.
    body = polygon([(21.0, 27.0), (43.0, 27.0), (45.5, 29.5), (52.0, 51.0), (49.5, 54.0),
                    (14.5, 54.0), (12.0, 51.0), (18.5, 29.5)])
    handle = ring(32.0, 18.5, 9.6, 4.4)
    groove = segment(17.5, 47.5, 46.5, 47.5, 0.8)
    return cut(union(body, handle), groove)


def encumbrance_sack():
    # Alternative: a full sack, tied at the neck. Reads too much like a gold pouch.
    body = smooth_union(circle(32.0, 41.5, 15.5),
                        polygon([(23.0, 30.0), (41.0, 30.0), (35.5, 22.5), (28.5, 22.5)]), 5.0)
    top = polygon([(27.0, 18.6), (37.0, 18.6), (42.5, 9.0), (36.0, 12.8), (32.0, 7.5), (28.0, 12.8), (21.5, 9.0)])
    sack = cut(union(body, top), union(segment(20, 18.0, 44, 18.0, 0.75), segment(20, 22.9, 44, 22.9, 0.75)))
    tie = box(32.0, 20.45, 7.0, 1.55, 0.8)
    seam = tapered([(24.8, 34.0), (23.0, 42.0), (25.2, 50.5)], 0.95, 0.75)
    return cut(union(sack, tie), seam)


GLYPHS = {
    "hunger": hunger,
    "cold": cold,
    "fatigue": fatigue,
    "stress": stress,
    "dirt": dirt,
    "encumbrance": encumbrance,
}

# Drawn too, for comparison, but not built into the mod.
ALTERNATES = {
    "dirt_splash": dirt_splash,
    "encumbrance_sack": encumbrance_sack,
}


# --- tracing -------------------------------------------------------------------

def trace(field):
    """Closed outlines of field < 0, as lists of (x, y)."""
    inside = field < 0
    if inside[0, :].any() or inside[-1, :].any() or inside[:, 0].any() or inside[:, -1].any():
        raise SystemExit("glyph touches the canvas edge")
    h = CANVAS / N

    def point(key):
        kind, j, i = key
        if kind == "h":   # grid edge from (i, j) to (i + 1, j)
            a, b = field[j, i], field[j, i + 1]
            return ((i + a / (a - b)) * h, j * h)
        a, b = field[j, i], field[j + 1, i]   # "v": from (i, j) to (i, j + 1)
        return (i * h, (j + a / (a - b)) * h)

    tl, tr = inside[:-1, :-1], inside[:-1, 1:]
    br, bl = inside[1:, 1:], inside[1:, :-1]
    case = tl * 8 + tr * 4 + br * 2 + bl * 1
    links = {}

    def link(a, b):
        links.setdefault(a, []).append(b)
        links.setdefault(b, []).append(a)

    for j, i in np.argwhere((case != 0) & (case != 15)):
        top, right = ("h", j, i), ("v", j, i + 1)
        bottom, left = ("h", j + 1, i), ("v", j, i)
        c = case[j, i]
        centre_in = (field[j, i] + field[j, i + 1] + field[j + 1, i] + field[j + 1, i + 1]) < 0
        pairs = {
            1: [(left, bottom)], 2: [(bottom, right)], 3: [(left, right)],
            4: [(top, right)], 6: [(top, bottom)], 7: [(top, left)],
            8: [(top, left)], 9: [(top, bottom)], 11: [(top, right)],
            12: [(left, right)], 13: [(right, bottom)], 14: [(left, bottom)],
            5: [(top, left), (right, bottom)] if centre_in else [(top, right), (left, bottom)],
            10: [(top, right), (left, bottom)] if centre_in else [(top, left), (right, bottom)],
        }[c]
        for a, b in pairs:
            link(a, b)

    loops, seen = [], set()
    for start in links:
        if start in seen:
            continue
        loop, prev, cur = [], None, start
        while True:
            seen.add(cur)
            loop.append(point(cur))
            nxt = [k for k in links[cur] if k != prev]
            if len(links[cur]) != 2:
                raise SystemExit("broken outline")
            prev, cur = cur, nxt[0]
            if cur == start:
                break
        # Sharp tips (the moon's horns) can leave slivers with next to no area.
        if abs(area(loop)) >= MIN_AREA:
            loops.append(loop)
    return loops


def area(loop):
    return sum(loop[k][0] * loop[k - 1][1] - loop[k - 1][0] * loop[k][1] for k in range(len(loop))) / -2.0


def simplify(points):
    """Ramer-Douglas-Peucker on a closed loop."""
    def rdp(pts):
        if len(pts) < 3:
            return pts
        (x0, y0), (x1, y1) = pts[0], pts[-1]
        dx, dy = x1 - x0, y1 - y0
        norm = math.hypot(dx, dy) or 1e-12
        best, at = -1.0, 0
        for k in range(1, len(pts) - 1):
            d = abs(dy * (pts[k][0] - x0) - dx * (pts[k][1] - y0)) / norm
            if d > best:
                best, at = d, k
        if best <= TOLERANCE:
            return [pts[0], pts[-1]]
        return rdp(pts[:at + 1])[:-1] + rdp(pts[at:])

    far = max(range(len(points)), key=lambda k: (points[k][0] - points[0][0]) ** 2 + (points[k][1] - points[0][1]) ** 2)
    first = rdp(points[:far + 1])
    second = rdp(points[far:] + [points[0]])
    return first[:-1] + second[:-1]


def path(loops, fill, opacity=None):
    d = " ".join("M" + " L".join("%.2f %.2f" % p for p in simplify(loop)) + " Z" for loop in loops)
    alpha = "" if opacity is None else ' fill-opacity="%.2f"' % opacity
    return '  <path fill="%s"%s fill-rule="evenodd" d="%s"/>\n' % (fill, alpha, d)


def to_svg(field):
    body = ""
    for inner, outer, opacity in HALO:
        body += path(trace(np.maximum(field - outer, -(field - inner))), "#000000", opacity)
    body += path(trace(field), "#FFFFFF")
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">\n' + body + "</svg>\n"


def main():
    for name, make in list(GLYPHS.items()) + list(ALTERNATES.items()):
        field = make()
        # The shipped glyphs now come from import_icons.py (game-icons.net, TEST3 on), so the
        # hand-drawn set is kept apart in drawn/ rather than overwriting them.
        os.makedirs(os.path.join(HERE, "drawn"), exist_ok=True)
        with open(os.path.join(HERE, "drawn", name + ".svg"), "w", encoding="utf-8", newline="\n") as f:
            f.write(to_svg(field))
        loops = trace(field)
        print("%-12s %d outline(s), %d points" % (name, len(loops), sum(len(simplify(l)) for l in loops)))


if __name__ == "__main__":
    main()
