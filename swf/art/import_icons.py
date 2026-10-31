"""Turn the game-icons.net glyphs in game-icons/ into the widget's art.

    python import_icons.py

For each icon: read its path (any SVG path commands, filled with the non-zero
rule as game-icons.net draws them), centre it on its own outline and fit its
longer side to FIT units, then build a signed distance field from the exact
outline and hand it to make_art.to_svg. That traces it into clean outlines with
no overlapping pieces and adds the same soft dark edge (make_art.HALO) the
hand-drawn set had, so the icons fade evenly and read over snow and sky.

Output: <name>.svg next to this file, 64 x 64 units, which build_icons.py turns
into Interface/NeedsWidget/icons/<name>.swf. Credits: game-icons/README.md.
"""

import math
import os
import re
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_art  # noqa: E402  (its grid, tracer and halo)

SOURCE_DIR = os.path.join(HERE, "game-icons")
ICONS = {
    "hunger": "lorc_chicken-leg",
    "cold": "lorc_snowflake-2",
    "fatigue": "delapouite_night-sleep",
    "stress": "lorc_screaming",
    "dirt": "delapouite_soap",
    "encumbrance": "delapouite_backpack",
    "thirst": "delapouite_waterskin",   # Tom, 2026-10-21 (art-options/thirst.jpg)
    "wetness": "lorc_droplets",         # Tom, 2026-10-21 (art-options/wetness.jpg)
    "vitality": "delapouite_heart-beats",              # Tom kept it, 2026-10-21 (art-options/vitality.jpg)
    "disease": "delapouite_plague-doctor-profile",     # Tom kept it, 2026-10-21 (art-options/disease.jpg)
    # Extra icons the main mod ships as icon files (SKSE/Plugins/NeedsWidget/Icons/).
    # Picked by Claude 2026-10-23 from art-options/extra-icons.png, for Tom to confirm.
    "light": "lorc_sun",
    "lantern": "delapouite_old-lantern",
    "wounds": "lorc_bandaged",
    "injury": "lorc_broken-bone",
    "vampire": "delapouite_vampire-dracula",
    "lute": "lorc_lyre",              # Claude, 2026-10-27 (HB700, Skyrim's Got Talent); no lute on game-icons
    "flute": "delapouite_flute",
    "drum": "delapouite_drum",
    "strength": "delapouite_biceps",     # Claude, 2026-10-31 (Strength/Agility/Intelligence Matters), for Tom to confirm
    "agility": "lorc_sprint",
    "intelligence": "lorc_brain",
    "magicdebuffs": "lorc_cursed-star",  # Claude, 2026-10-27 (CenteredSword, Rogues 'n Raiders)  # Claude, 2026-10-27 (Raynor1511), for Tom to confirm
}
FIT = 52.0            # the longer side of every glyph, in canvas units (canvas is 64)
CENTRE = 32.0
CURVE_STEPS = 16      # line pieces per curve
ARC_STEP = math.radians(6)


# --- SVG path data ---------------------------------------------------------------

NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


class Scanner:
    def __init__(self, d):
        self.d, self.i = d, 0

    def skip(self):
        while self.i < len(self.d) and self.d[self.i] in " \t\r\n,":
            self.i += 1

    def command(self):
        self.skip()
        if self.i < len(self.d) and self.d[self.i].isalpha():
            self.i += 1
            return self.d[self.i - 1]
        return None

    def more(self):
        self.skip()
        return self.i < len(self.d) and not self.d[self.i].isalpha()

    def number(self):
        self.skip()
        m = NUMBER.match(self.d, self.i)
        if not m:
            raise SystemExit("bad path data at %d: %r" % (self.i, self.d[self.i:self.i + 20]))
        self.i = m.end()
        return float(m.group())

    def flag(self):
        # Arc flags are a single 0 or 1 and may be written without a separator.
        self.skip()
        c = self.d[self.i]
        if c not in "01":
            raise SystemExit("bad arc flag at %d" % self.i)
        self.i += 1
        return c == "1"


def cubic(p0, c1, c2, p3):
    out = []
    for k in range(1, CURVE_STEPS + 1):
        t = k / CURVE_STEPS
        mt = 1 - t
        out.append((mt ** 3 * p0[0] + 3 * mt * mt * t * c1[0] + 3 * mt * t * t * c2[0] + t ** 3 * p3[0],
                    mt ** 3 * p0[1] + 3 * mt * mt * t * c1[1] + 3 * mt * t * t * c2[1] + t ** 3 * p3[1]))
    return out


def quad(p0, c, p2):
    return cubic(p0, (p0[0] + 2 / 3 * (c[0] - p0[0]), p0[1] + 2 / 3 * (c[1] - p0[1])),
                 (p2[0] + 2 / 3 * (c[0] - p2[0]), p2[1] + 2 / 3 * (c[1] - p2[1])), p2)


def arc(p0, rx, ry, rotation, large, sweep, p1):
    # SVG 1.1 implementation notes F.6.5: endpoint to centre parameterisation.
    if p0 == p1:
        return []
    rx, ry = abs(rx), abs(ry)
    if rx == 0 or ry == 0:
        return [p1]
    phi = math.radians(rotation)
    cp, sp = math.cos(phi), math.sin(phi)
    dx, dy = (p0[0] - p1[0]) / 2, (p0[1] - p1[1]) / 2
    x1, y1 = cp * dx + sp * dy, -sp * dx + cp * dy
    scale = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry)
    if scale > 1:
        rx, ry = rx * math.sqrt(scale), ry * math.sqrt(scale)
    num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1
    den = rx * rx * y1 * y1 + ry * ry * x1 * x1
    co = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cx1, cy1 = co * rx * y1 / ry, -co * ry * x1 / rx
    cx = cp * cx1 - sp * cy1 + (p0[0] + p1[0]) / 2
    cy = sp * cx1 + cp * cy1 + (p0[1] + p1[1]) / 2

    def angle(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a

    t1 = angle(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry)
    dt = angle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    steps = max(2, math.ceil(abs(dt) / ARC_STEP))
    out = []
    for k in range(1, steps + 1):
        t = t1 + dt * k / steps
        x, y = rx * math.cos(t), ry * math.sin(t)
        out.append((cp * x - sp * y + cx, sp * x + cp * y + cy))
    out[-1] = p1
    return out


def subpaths(d):
    """Every subpath as a list of points (closed implicitly when filled)."""
    s = Scanner(d)
    paths, cur = [], []
    pos = start = (0.0, 0.0)
    last_cubic = last_quad = None
    cmd = None
    while True:
        c = s.command()
        if c is None:
            if not s.more():
                break
            if cmd is None:
                raise SystemExit("path data must start with a command")
            c = cmd
        cmd = c
        rel = c.islower()
        kind = c.upper()

        def pt():
            x, y = s.number(), s.number()
            return (x + pos[0], y + pos[1]) if rel else (x, y)

        if kind == "Z":
            if len(cur) > 1:
                paths.append(cur)
            cur, pos = [], start
            last_cubic = last_quad = None
            continue
        while True:
            if kind != "M" and not cur:
                cur = [pos]   # drawing on after a close starts at the closed subpath's start
            if kind == "M":
                if len(cur) > 1:
                    paths.append(cur)
                pos = start = pt()
                cur = [pos]
                kind = "L"   # further pairs after a move are lines
                last_cubic = last_quad = None
            elif kind == "L":
                pos = pt()
                cur.append(pos)
                last_cubic = last_quad = None
            elif kind == "H":
                x = s.number()
                pos = (x + pos[0] if rel else x, pos[1])
                cur.append(pos)
                last_cubic = last_quad = None
            elif kind == "V":
                y = s.number()
                pos = (pos[0], y + pos[1] if rel else y)
                cur.append(pos)
                last_cubic = last_quad = None
            elif kind in "CS":
                if kind == "C":
                    c1 = pt()
                else:
                    c1 = (2 * pos[0] - last_cubic[0], 2 * pos[1] - last_cubic[1]) if last_cubic else pos
                c2, end = pt(), pt()
                cur.extend(cubic(pos, c1, c2, end))
                pos, last_cubic, last_quad = end, c2, None
            elif kind in "QT":
                if kind == "Q":
                    ctl = pt()
                else:
                    ctl = (2 * pos[0] - last_quad[0], 2 * pos[1] - last_quad[1]) if last_quad else pos
                end = pt()
                cur.extend(quad(pos, ctl, end))
                pos, last_quad, last_cubic = end, ctl, None
            elif kind == "A":
                rx, ry, rot = s.number(), s.number(), s.number()
                large, sweep = s.flag(), s.flag()
                end = pt()
                cur.extend(arc(pos, rx, ry, rot, large, sweep, end))
                pos = end
                last_cubic = last_quad = None
            else:
                raise SystemExit("unsupported path command %s" % c)
            if not s.more():
                break
        if c in "Mm":
            cmd = "l" if rel else "L"
    if len(cur) > 1:
        paths.append(cur)
    return paths


def read_glyph(file):
    text = open(file, encoding="utf-8").read()
    if "transform=" in text or "<g" in text:
        raise SystemExit("%s: groups and transforms are not handled" % file)
    found = []
    for m in re.finditer(r"<path\b([^>]*)>", text):
        attrs = dict(re.findall(r'([\w:-]+)\s*=\s*"([^"]*)"', m.group(1)))
        if attrs.get("fill", "").lower() in ("#fff", "#ffffff"):
            found.append(attrs["d"])
        elif attrs.get("d") != "M0 0h512v512H0z":
            raise SystemExit("%s: unexpected path %r" % (file, attrs.get("d", "")[:40]))
    if len(found) != 1:
        raise SystemExit("%s: expected one white glyph path, found %d" % (file, len(found)))
    return subpaths(found[0])


# --- field ---------------------------------------------------------------------

def fit(paths):
    xs = [p[0] for path in paths for p in path]
    ys = [p[1] for path in paths for p in path]
    k = FIT / max(max(xs) - min(xs), max(ys) - min(ys))
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    return [[(CENTRE + (x - cx) * k, CENTRE + (y - cy) * k) for x, y in path] for path in paths]


def edges(paths):
    a, b = [], []
    for path in paths:
        for k in range(len(path)):
            p, q = path[k], path[(k + 1) % len(path)]   # the closing edge too
            if p != q:
                a.append(p)
                b.append(q)
    return np.array(a), np.array(b)


def winding(a, b):
    """Non-zero winding number at every grid node, by crossings of a ray to the right."""
    n, h = make_art.N, make_art.CANVAS / make_art.N
    diff = np.zeros((n + 1, n + 2), dtype=np.int32)
    for (x0, y0), (x1, y1) in zip(a, b):
        if y0 == y1:
            continue
        lo, hi = min(y0, y1), max(y0, y1)
        rows = np.arange(math.ceil(lo / h), math.ceil(hi / h))
        rows = rows[(rows >= 0) & (rows <= n)]
        rows = rows[(rows * h >= lo) & (rows * h < hi)]
        if rows.size == 0:
            continue
        xc = x0 + (rows * h - y0) * (x1 - x0) / (y1 - y0)
        first_right = np.clip(np.ceil(xc / h).astype(np.int64), 0, n + 1)
        direction = 1 if y1 > y0 else -1
        np.add.at(diff, (rows, 0), direction)
        np.add.at(diff, (rows, first_right), -direction)
    return np.cumsum(diff, axis=1)[:, :n + 1]


def distance(a, b):
    """Distance from every grid node to the nearest edge."""
    pts = np.stack([make_art.X.ravel(), make_art.Y.ravel()], axis=1).astype(np.float32)
    a = a.astype(np.float32)
    ab = (b - a).astype(np.float32)
    len2 = (ab * ab).sum(axis=1)
    out = np.empty(len(pts), dtype=np.float32)
    chunk = 2048
    for s in range(0, len(pts), chunk):
        p = pts[s:s + chunk]
        apx = p[:, None, 0] - a[None, :, 0]
        apy = p[:, None, 1] - a[None, :, 1]
        t = np.clip((apx * ab[None, :, 0] + apy * ab[None, :, 1]) / len2[None, :], 0.0, 1.0)
        dx = apx - t * ab[None, :, 0]
        dy = apy - t * ab[None, :, 1]
        out[s:s + chunk] = np.sqrt((dx * dx + dy * dy).min(axis=1))
    return out.reshape(make_art.X.shape).astype(np.float64)


def field_for(file):
    a, b = edges(fit(read_glyph(file)))
    inside = winding(a, b) != 0
    d = distance(a, b)
    return np.where(inside, -d, d)


def main():
    for name, source in ICONS.items():
        started = time.time()
        field = field_for(os.path.join(SOURCE_DIR, source + ".svg"))
        with open(os.path.join(HERE, name + ".svg"), "w", encoding="utf-8", newline="\n") as f:
            f.write(make_art.to_svg(field))
        loops = make_art.trace(field)
        pts = [p for loop in loops for p in loop]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        print("%-12s from %-26s %3d outline(s), %5d points, x %.1f-%.1f y %.1f-%.1f, %.1fs" % (
            name, source, len(loops), sum(len(make_art.simplify(l)) for l in loops),
            min(xs), max(xs), min(ys), max(ys), time.time() - started))


if __name__ == "__main__":
    main()
