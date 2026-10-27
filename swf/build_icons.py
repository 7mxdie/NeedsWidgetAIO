"""Build the pictures in Interface/NeedsWidget/icons/.

    python build_icons.py

    <name>.swf        one glyph per need, from swf/art/<name>.svg
    frame_ring.swf    the frame behind each glyph for the Ring look
    frame_bar.swf     the frame behind each glyph for the Bar look

The pictures hold shapes only, no code, so a reskin can swap them without ever
touching widget.swf. All of them are drawn on a 64 x 64 canvas, origin top left.
widget.swf places a frame with its centre (32, 32) on the icon's centre, and
scales a glyph's canvas into the art box set in NeedsWidget.ini.

Glyphs read a small SVG subset: <path> elements with d (M L H V Q C Z, absolute
or relative), a solid fill (#RRGGBB, default white) and fill-opacity, in a
viewBox of 0 0 64 64. Each <path> becomes its own DefineShape3, placed in
document order, so later paths paint over earlier ones. Inside one path,
contours are re-oriented by how deeply they nest (outer ones clockwise on
screen, holes anticlockwise) and filled on their right-hand side, so holes come
out right whatever winding rule the renderer uses.
"""

import hashlib
import math
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ART_DIR = os.path.join(HERE, "art")
OUT_DIR = os.path.join(HERE, "build", "Interface", "NeedsWidget", "icons")
NAMES = ["hunger", "cold", "fatigue", "stress", "dirt", "encumbrance", "thirst", "wetness", "vitality", "disease",
         "light", "lantern", "wounds", "injury", "vampire"]  # the last five are the shipped extra icons' art
CANVAS = 64
TWIPS = 20
OFFWHITE = (236, 233, 226)


# --- SVG -----------------------------------------------------------------------

NUM = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
TOKEN = re.compile(r"[MmLlHhVvQqCcZz]|" + NUM)


def cubic_to_quads(p0, c1, c2, p3, pieces=4):
    # Split the cubic into pieces and fit one quadratic to each.
    def at(t):
        mt = 1 - t
        return (mt ** 3 * p0[0] + 3 * mt * mt * t * c1[0] + 3 * mt * t * t * c2[0] + t ** 3 * p3[0],
                mt ** 3 * p0[1] + 3 * mt * mt * t * c1[1] + 3 * mt * t * t * c2[1] + t ** 3 * p3[1])

    def slope(t):
        mt = 1 - t
        return (3 * mt * mt * (c1[0] - p0[0]) + 6 * mt * t * (c2[0] - c1[0]) + 3 * t * t * (p3[0] - c2[0]),
                3 * mt * mt * (c1[1] - p0[1]) + 6 * mt * t * (c2[1] - c1[1]) + 3 * t * t * (p3[1] - c2[1]))

    out = []
    for k in range(pieces):
        t0, t1 = k / pieces, (k + 1) / pieces
        a, b = at(t0), at(t1)
        da, db = slope(t0), slope(t1)
        h = (t1 - t0) / 3
        # Control points of this piece as a cubic, then the usual quadratic fit.
        q1 = (a[0] + da[0] * h, a[1] + da[1] * h)
        q2 = (b[0] - db[0] * h, b[1] - db[1] * h)
        control = ((3 * (q1[0] + q2[0]) - a[0] - b[0]) / 4, (3 * (q1[1] + q2[1]) - a[1] - b[1]) / 4)
        out.append(("Q", control, b))
    return out


def parse_path(d, where):
    """List of closed contours: {"start": (x, y), "segs": [("L", p) | ("Q", c, p)]}."""
    toks = TOKEN.findall(d)
    contours, cur = [], None
    pos = start = (0.0, 0.0)
    cmd, i = None, 0

    def num():
        nonlocal i
        if i >= len(toks) or toks[i].isalpha():
            sys.exit("%s: path data ends early" % where)
        i += 1
        return float(toks[i - 1])

    def close():
        if cur is not None:
            contours.append(cur)

    while i < len(toks):
        if toks[i].isalpha():
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                close()
                cur, pos = None, start
                continue
        elif cmd is None:
            sys.exit("%s: path data must start with a command" % where)
        rel = cmd.islower()
        ox, oy = pos if rel else (0.0, 0.0)
        kind = cmd.upper()
        if kind == "M":
            close()
            pos = start = (num() + ox, num() + oy)
            cur = {"start": pos, "segs": []}
            cmd = "l" if rel else "L"
            continue
        if cur is None:
            cur = {"start": pos, "segs": []}
            start = pos
        if kind == "L":
            pos = (num() + ox, num() + oy)
            cur["segs"].append(("L", pos))
        elif kind == "H":
            pos = (num() + ox, pos[1])
            cur["segs"].append(("L", pos))
        elif kind == "V":
            pos = (pos[0], num() + oy)
            cur["segs"].append(("L", pos))
        elif kind == "Q":
            c = (num() + ox, num() + oy)
            pos = (num() + ox, num() + oy)
            cur["segs"].append(("Q", c, pos))
        elif kind == "C":
            c1 = (num() + ox, num() + oy)
            c2 = (num() + ox, num() + oy)
            end = (num() + ox, num() + oy)
            cur["segs"].extend(cubic_to_quads(pos, c1, c2, end))
            pos = end
        else:
            sys.exit("%s: unsupported path command %s" % (where, cmd))
    close()
    return contours


def read_svg(name):
    """[(fill, contours)] for every <path>, fill = ("solid", (r, g, b, a))."""
    file = os.path.join(ART_DIR, name + ".svg")
    text = open(file, encoding="utf-8").read()
    if re.search(r"<(g|use|rect|circle|ellipse|polygon|polyline|line|text|image)\b", text) or "transform=" in text:
        sys.exit("%s: only plain <path> elements are supported (no groups, shapes or transforms)" % file)
    view = re.search(r'viewBox="([^"]+)"', text)
    if not view or [float(v) for v in view.group(1).split()] != [0, 0, CANVAS, CANVAS]:
        sys.exit("%s: viewBox must be 0 0 %d %d" % (file, CANVAS, CANVAS))
    shapes = []
    for m in re.finditer(r"<path\b([^>]*)>", text, re.S):
        attrs = dict(re.findall(r'([\w:-]+)\s*=\s*"([^"]*)"', m.group(1)))
        fill = attrs.get("fill", "#FFFFFF")
        if not re.fullmatch(r"#[0-9A-Fa-f]{6}", fill):
            sys.exit("%s: fill must be #RRGGBB, got %r" % (file, fill))
        alpha = round(255 * min(1.0, max(0.0, float(attrs.get("fill-opacity", "1")))))
        rgb = int(fill[1:], 16)
        shapes.append((("solid", (rgb >> 16, (rgb >> 8) & 255, rgb & 255, alpha)),
                       parse_path(attrs.get("d", ""), file)))
    if not shapes:
        sys.exit("%s: no <path> found" % file)
    return shapes


# --- frames ----------------------------------------------------------------------

def circle_contour(cx, cy, r, pieces=16):
    # Quadratic arcs of 22.5 degrees: within 0.2% of a true circle.
    step = 2 * math.pi / pieces
    k = r / math.cos(step / 2)
    segs = []
    for n in range(pieces):
        mid, end = step * (n + 0.5), step * (n + 1)
        segs.append(("Q", (cx + math.sin(mid) * k, cy - math.cos(mid) * k), (cx + math.sin(end) * r, cy - math.cos(end) * r)))
    return {"start": (cx, cy - r), "segs": segs}


def poly_contour(points):
    return {"start": points[0], "segs": [("L", p) for p in points[1:]]}


def rect_contour(x0, y0, x1, y1):
    return poly_contour([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])


def diamond_contour(cx, cy, r):
    return poly_contour([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)])


def soft_backing(cx, cy, r, alpha):
    # A dark disc that fades to nothing at its edge, like the vanilla HUD's own
    # soft backings: alpha * (1 - d^2)^1.5 across the radius.
    stops = []
    for d in (0.0, 0.25, 0.5, 0.7, 0.85, 1.0):
        stops.append((round(d * 255), (0, 0, 0, round(255 * alpha * (1 - d * d) ** 1.5))))
    return (("radial", stops, (cx, cy), r), [circle_contour(cx, cy, r)])


def frame_ring():
    # Ring look: soft backing, a faint track round the glyph, a diamond at 12
    # o'clock where the meter starts. widget.swf draws the meter over the track.
    return [
        soft_backing(32, 32, 33, 0.50),
        (("solid", (255, 255, 255, round(255 * 0.16))), [circle_contour(32, 32, 28.4), circle_contour(32, 32, 26.6)]),
        (("solid", OFFWHITE + (230,)), [diamond_contour(32, 4.5, 2.6)]),
    ]


def frame_bar():
    # Bar look: soft backing behind the glyph (raised 4), a small vanilla-style
    # bar under it with diamond ends. widget.swf draws the fill inside the bar.
    return [
        soft_backing(32, 28, 30, 0.45),
        (("solid", (0, 0, 0, round(255 * 0.55))), [rect_contour(15, 53, 49, 55.4)]),
        (("solid", (255, 255, 255, round(255 * 0.30))), [rect_contour(15, 53, 49, 55.4), rect_contour(15.5, 53.5, 48.5, 54.9)]),
        (("solid", OFFWHITE + (200,)), [diamond_contour(13.4, 54.2, 2.0), diamond_contour(50.6, 54.2, 2.0)]),
    ]


FRAMES = {"frame_ring": frame_ring, "frame_bar": frame_bar}


# --- contour orientation -------------------------------------------------------

def flatten(contour):
    pts, pos = [contour["start"]], contour["start"]
    for seg in contour["segs"]:
        if seg[0] == "L":
            pos = seg[1]
            pts.append(pos)
        else:
            c, end = seg[1], seg[2]
            for k in range(1, 9):
                t = k / 8
                pts.append(((1 - t) ** 2 * pos[0] + 2 * (1 - t) * t * c[0] + t * t * end[0],
                            (1 - t) ** 2 * pos[1] + 2 * (1 - t) * t * c[1] + t * t * end[1]))
            pos = end
    return pts


def signed_area(pts):
    # Positive = clockwise on screen (y down).
    return sum(pts[k - 1][0] * pts[k][1] - pts[k][0] * pts[k - 1][1] for k in range(len(pts))) / 2


def contains(pts, p):
    inside, j = False, len(pts) - 1
    for i in range(len(pts)):
        (xi, yi), (xj, yj) = pts[i], pts[j]
        if (yi > p[1]) != (yj > p[1]) and p[0] < (xj - xi) * (p[1] - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def reverse(contour):
    nodes = [contour["start"]] + [seg[-1] for seg in contour["segs"]]
    segs = []
    for k in range(len(contour["segs"]) - 1, -1, -1):
        seg = contour["segs"][k]
        segs.append(("L", nodes[k]) if seg[0] == "L" else ("Q", seg[1], nodes[k]))
    return {"start": nodes[-1], "segs": segs}


def orient(contours):
    flat = [flatten(c) for c in contours]
    out = []
    for k, c in enumerate(contours):
        depth = sum(1 for m in range(len(contours)) if m != k and contains(flat[m], flat[k][0]))
        clockwise = signed_area(flat[k]) > 0
        out.append(c if clockwise == (depth % 2 == 0) else reverse(c))
    return out


# --- SWF encoding --------------------------------------------------------------

class Bits:
    def __init__(self):
        self.bits = []

    def u(self, value, n):
        for k in range(n - 1, -1, -1):
            self.bits.append((value >> k) & 1)

    def s(self, value, n):
        self.u(value & ((1 << n) - 1), n)

    def bytes(self):
        bits = self.bits + [0] * (-len(self.bits) % 8)
        return bytes(int("".join(map(str, bits[k:k + 8])), 2) for k in range(0, len(bits), 8))


def sbits(*values):
    return max([1] + [(v if v >= 0 else ~v).bit_length() + 1 for v in values])


def rect(xmin, xmax, ymin, ymax):
    b = Bits()
    n = sbits(xmin, xmax, ymin, ymax)
    b.u(n, 5)
    for v in (xmin, xmax, ymin, ymax):
        b.s(v, n)
    return b.bytes()


def tag(code, body):
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def tw(p):
    return (round(p[0] * TWIPS), round(p[1] * TWIPS))


def fill_style(fill):
    if fill[0] == "solid":
        return bytes([0x00]) + bytes(fill[1])
    # Radial gradient: the gradient square (+-16384 twips) is scaled so ratio 255
    # lands on the radius, and moved to the centre.
    _, stops, centre, radius = fill
    scale = round(radius * TWIPS / 16384 * 65536)
    tx, ty = tw(centre)
    m = Bits()
    m.u(1, 1)                      # HasScale
    n = sbits(scale)
    m.u(n, 5)
    m.s(scale, n)
    m.s(scale, n)
    m.u(0, 1)                      # HasRotate
    n = sbits(tx, ty)
    m.u(n, 5)
    m.s(tx, n)
    m.s(ty, n)
    if len(stops) > 15:
        sys.exit("a gradient takes at most 15 stops")
    grad = bytes([len(stops)])     # pad spread, normal interpolation, count
    for ratio, rgba in stops:
        grad += bytes([ratio]) + bytes(rgba)
    return bytes([0x12]) + m.bytes() + grad


def define_shape(shape_id, fill, contours):
    b = Bits()
    b.u(1, 4)    # NumFillBits
    b.u(0, 4)    # NumLineBits
    xs, ys = [], []
    first = True
    pos = None
    for c in orient(contours):
        sx, sy = tw(c["start"])
        # StyleChangeRecord: move to the contour's start, fill on the right.
        b.u(0, 1)
        b.u(0, 1)                      # StateNewStyles
        b.u(0, 1)                      # StateLineStyle
        b.u(1 if first else 0, 1)      # StateFillStyle1
        b.u(0, 1)                      # StateFillStyle0
        b.u(1, 1)                      # StateMoveTo
        n = sbits(sx, sy)
        b.u(n, 5)
        b.s(sx, n)
        b.s(sy, n)
        if first:
            b.u(1, 1)                  # FillStyle1 = 1
            first = False
        pos = (sx, sy)
        xs.append(sx)
        ys.append(sy)
        segs = list(c["segs"])
        if not segs or tw(segs[-1][-1]) != (sx, sy):
            segs.append(("L", c["start"]))
        for seg in segs:
            end = tw(seg[-1])
            if seg[0] == "L":
                dx, dy = end[0] - pos[0], end[1] - pos[1]
                if dx == 0 and dy == 0:
                    continue
                b.u(1, 1)
                b.u(1, 1)
                if dx != 0 and dy != 0:
                    n = max(2, sbits(dx, dy))
                    b.u(n - 2, 4)
                    b.u(1, 1)          # GeneralLineFlag
                    b.s(dx, n)
                    b.s(dy, n)
                else:
                    n = max(2, sbits(dx or dy))
                    b.u(n - 2, 4)
                    b.u(0, 1)
                    b.u(1 if dx == 0 else 0, 1)   # VertLineFlag
                    b.s(dy if dx == 0 else dx, n)
            else:
                ctl = tw(seg[1])
                cdx, cdy = ctl[0] - pos[0], ctl[1] - pos[1]
                adx, ady = end[0] - ctl[0], end[1] - ctl[1]
                if cdx == cdy == adx == ady == 0:
                    continue
                n = max(2, sbits(cdx, cdy, adx, ady))
                b.u(1, 1)
                b.u(0, 1)
                b.u(n - 2, 4)
                for v in (cdx, cdy, adx, ady):
                    b.s(v, n)
                xs.append(ctl[0])
                ys.append(ctl[1])
            if n > 17:
                sys.exit("edge too long for one record")
            pos = end
            xs.append(end[0])
            ys.append(end[1])
    b.u(0, 6)                          # EndShapeRecord
    bounds = (min(xs), max(xs), min(ys), max(ys))
    body = struct.pack("<H", shape_id) + rect(*bounds)
    body += bytes([1]) + fill_style(fill)    # one fill style
    body += bytes([0])                       # no line styles
    body += b.bytes()
    return tag(32, body), bounds


def picture_swf(shapes):
    tags, placements, union = b"", b"", None
    for k, (fill, contours) in enumerate(shapes):
        shape_id = k + 1
        t, bounds = define_shape(shape_id, fill, contours)
        tags += t
        placements += tag(26, bytes([0x02]) + struct.pack("<HH", k + 1, shape_id))   # PlaceObject2: depth, id
        union = bounds if union is None else (min(union[0], bounds[0]), max(union[1], bounds[1]),
                                              min(union[2], bounds[2]), max(union[3], bounds[3]))
    body = rect(0, CANVAS * TWIPS, 0, CANVAS * TWIPS)
    body += struct.pack("<HH", 30 << 8, 1)            # 30 fps, 1 frame
    body += tag(69, struct.pack("<I", 0))             # FileAttributes: AS2, no network
    body += tags + placements
    body += tag(1, b"") + tag(0, b"")                 # ShowFrame, End
    return b"FWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + body, union


def fmt(twips):
    # The way AS2 prints Math.round(v * 100) / 100 for a twip value.
    return "%.15g" % (twips / TWIPS)


def build():
    """Build every picture. Returns {name: "xMin,yMin,xMax,yMax"} in canvas units."""
    os.makedirs(OUT_DIR, exist_ok=True)
    bounds = {}
    jobs = [(name, lambda n=name: read_svg(n)) for name in NAMES] + list(FRAMES.items())
    for name, make in jobs:
        data, (x0, x1, y0, y1) = picture_swf(make())
        with open(os.path.join(OUT_DIR, name + ".swf"), "wb") as f:
            f.write(data)
        bounds[name] = ",".join(fmt(v) for v in (x0, y0, x1, y1))
        print("picture %-12s %5d bytes  bounds %-26s sha256 %s" % (
            name, len(data), bounds[name], hashlib.sha256(data).hexdigest()[:16]))
    return bounds


if __name__ == "__main__":
    build()
