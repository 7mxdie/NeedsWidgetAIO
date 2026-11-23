"""Build the 3.0 icon styles from style_defs.py.

    python build_styles.py [--sheet]

    swf/build/Interface/NeedsWidget/icons/frame_<key>.swf   each style's frame, shapes only, no code
    swf/build/SKSE/Plugins/NeedsWidget/Styles/<key>.ini      what the dll reads: meter, glyph, colours (docs/STYLES.md)
    --sheet: swf/build/styles-check.html, every frame drawn back from the converted shapes with its meter
             (from the ini's own numbers) at a few levels, to compare with the mock-ups Tom picked from.

The frames are written in a small SVG subset: <g> (fill, opacity, transform), <path> (M L H V Q C A Z,
absolute), <circle>, <ellipse>, <rect> (rx, transform), <line>; fill and stroke as #rrggbb, none or
url(#gradient) from style_defs.GRADIENTS; fill-opacity, stroke-opacity, opacity, stroke-width,
stroke-dasharray; transform translate/scale/rotate. Anything else stops the build.
"""
import hashlib
import math
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import build_icons as bi  # noqa: E402
import style_defs  # noqa: E402

OUT_ICONS = os.path.join(HERE, "..", "build", "Interface", "NeedsWidget", "icons")
OUT_INI = os.path.join(HERE, "..", "build", "SKSE", "Plugins", "NeedsWidget", "Styles")
TWIPS = bi.TWIPS


# --- geometry ---------------------------------------------------------------------

def mat_mul(a, b):
    # 2x3 affine matrices (a, b, c, d, e, f): x' = a x + c y + e, y' = b x + d y + f.
    return (a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1], a[0] * b[2] + a[2] * b[3], a[1] * b[2] + a[3] * b[3],
            a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5])


IDENTITY = (1, 0, 0, 1, 0, 0)


def parse_transform(text):
    m = IDENTITY
    for name, args in re.findall(r"(\w+)\(([^)]*)\)", text or ""):
        v = [float(x) for x in re.split(r"[\s,]+", args.strip()) if x]
        if name == "translate":
            t = (1, 0, 0, 1, v[0], v[1] if len(v) > 1 else 0)
        elif name == "scale":
            t = (v[0], 0, 0, v[1] if len(v) > 1 else v[0], 0, 0)
        elif name == "rotate":
            a = math.radians(v[0])
            cx, cy = (v[1], v[2]) if len(v) > 2 else (0, 0)
            t = mat_mul((1, 0, 0, 1, cx, cy), mat_mul((math.cos(a), math.sin(a), -math.sin(a), math.cos(a), 0, 0), (1, 0, 0, 1, -cx, -cy)))
        else:
            sys.exit("unsupported transform " + name)
        m = mat_mul(m, t)
    return m


def apply(m, p):
    return (m[0] * p[0] + m[2] * p[1] + m[4], m[1] * p[0] + m[3] * p[1] + m[5])


def ellipse_arc(cx, cy, rx, ry, t0, t1):
    """Quadratic pieces along an axis-aligned ellipse from angle t0 to t1 (radians, y down)."""
    n = max(1, math.ceil(abs(t1 - t0) / (math.pi / 8)))
    step = (t1 - t0) / n
    k = 1 / math.cos(step / 2)
    segs = []
    for i in range(n):
        mid, end = t0 + step * (i + .5), t0 + step * (i + 1)
        segs.append(("Q", (cx + rx * k * math.cos(mid), cy + ry * k * math.sin(mid)), (cx + rx * math.cos(end), cy + ry * math.sin(end))))
    return segs


def arc_segments(p0, rx, ry, rot, large, sweep, p1):
    # SVG's endpoint arc to centre form (rotation 0 only, which is all the frames use).
    if rot:
        sys.exit("rotated arcs are not supported")
    if p0 == p1:
        return []
    x1 = (p0[0] - p1[0]) / 2
    y1 = (p0[1] - p1[1]) / 2
    lam = (x1 / rx) ** 2 + (y1 / ry) ** 2
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1
    den = rx * rx * y1 * y1 + ry * ry * x1 * x1
    co = math.sqrt(max(0, num / den)) * (-1 if large == sweep else 1)
    cxp, cyp = co * rx * y1 / ry, -co * ry * x1 / rx
    cx, cy = cxp + (p0[0] + p1[0]) / 2, cyp + (p0[1] + p1[1]) / 2
    t0 = math.atan2((y1 - cyp) / ry, (x1 - cxp) / rx)
    t1 = math.atan2((-y1 - cyp) / ry, (-x1 - cxp) / rx)
    dt = t1 - t0
    if sweep and dt < 0:
        dt += 2 * math.pi
    elif not sweep and dt > 0:
        dt -= 2 * math.pi
    segs = ellipse_arc(cx, cy, rx, ry, t0, t0 + dt)
    segs[-1] = (segs[-1][0], segs[-1][1], p1)
    return segs


TOKEN = re.compile(r"[MLHVQCAZmlhvqcaz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def parse_d(d):
    """[{"start", "segs", "closed"}], absolute commands only."""
    toks = TOKEN.findall(d)
    out, cur, pos, start, cmd, i = [], None, (0.0, 0.0), (0.0, 0.0), None, 0

    def num():
        nonlocal i
        i += 1
        return float(toks[i - 1])

    while i < len(toks):
        if toks[i].isalpha():
            cmd = toks[i]
            i += 1
            if cmd.islower():
                sys.exit("relative path commands are not supported: " + d[:40])
            if cmd == "Z":
                if cur:
                    cur["closed"] = True
                    out.append(cur)
                cur, pos = None, start
                continue
        if cmd == "M":
            if cur:
                out.append(cur)
            pos = start = (num(), num())
            cur = {"start": pos, "segs": [], "closed": False}
            cmd = "L"
            continue
        if cur is None:
            cur = {"start": pos, "segs": [], "closed": False}
            start = pos
        if cmd == "L":
            pos = (num(), num())
            cur["segs"].append(("L", pos))
        elif cmd == "H":
            pos = (num(), pos[1])
            cur["segs"].append(("L", pos))
        elif cmd == "V":
            pos = (pos[0], num())
            cur["segs"].append(("L", pos))
        elif cmd == "Q":
            c = (num(), num())
            pos = (num(), num())
            cur["segs"].append(("Q", c, pos))
        elif cmd == "C":
            c1, c2, end = (num(), num()), (num(), num()), (num(), num())
            cur["segs"].extend(bi.cubic_to_quads(pos, c1, c2, end))
            pos = end
        elif cmd == "A":
            rx, ry, rot, large, sweep = num(), num(), num(), num(), num()
            end = (num(), num())
            cur["segs"].extend(arc_segments(pos, rx, ry, rot, int(large), int(sweep), end))
            pos = end
        else:
            sys.exit("unsupported path command " + cmd)
    if cur:
        out.append(cur)
    return out


def circle_contours(cx, cy, rx, ry):
    return [{"start": (cx + rx, cy), "segs": ellipse_arc(cx, cy, rx, ry, 0, 2 * math.pi), "closed": True}]


def rect_contours(x, y, w, h, rx):
    if rx <= 0:
        return [{"start": (x, y), "segs": [("L", (x + w, y)), ("L", (x + w, y + h)), ("L", (x, y + h))], "closed": True}]
    rx = min(rx, w / 2, h / 2)
    segs = [("L", (x + w - rx, y))] + ellipse_arc(x + w - rx, y + rx, rx, rx, -math.pi / 2, 0)
    segs += [("L", (x + w, y + h - rx))] + ellipse_arc(x + w - rx, y + h - rx, rx, rx, 0, math.pi / 2)
    segs += [("L", (x + rx, y + h))] + ellipse_arc(x + rx, y + h - rx, rx, rx, math.pi / 2, math.pi)
    segs += [("L", (x, y + rx))] + ellipse_arc(x + rx, y + rx, rx, rx, math.pi, 1.5 * math.pi)
    return [{"start": (x + rx, y), "segs": segs, "closed": True}]


def transform_contours(m, contours):
    out = []
    for c in contours:
        segs = [("L", apply(m, s[1])) if s[0] == "L" else ("Q", apply(m, s[1]), apply(m, s[2])) for s in c["segs"]]
        out.append({"start": apply(m, c["start"]), "segs": segs, "closed": c["closed"]})
    return out


def flatten(c, steps=8):
    pts, pos = [c["start"]], c["start"]
    for s in c["segs"]:
        if s[0] == "L":
            pos = s[1]
            pts.append(pos)
        else:
            for k in range(1, steps + 1):
                t = k / steps
                pts.append(((1 - t) ** 2 * pos[0] + 2 * (1 - t) * t * s[1][0] + t * t * s[2][0],
                            (1 - t) ** 2 * pos[1] + 2 * (1 - t) * t * s[1][1] + t * t * s[2][1]))
            pos = s[2]
    if c["closed"] and pts[-1] != pts[0]:
        pts.append(pts[0])
    return pts


def dashes(points, pattern):
    """Split a polyline into dash pieces (open contours of lines)."""
    out, on, k, left = [], True, 0, pattern[0]
    cur = [points[0]]
    for a, b in zip(points, points[1:]):
        seg = math.dist(a, b)
        t = 0
        while seg - t > left:
            t += left
            p = (a[0] + (b[0] - a[0]) * t / seg, a[1] + (b[1] - a[1]) * t / seg)
            if on:
                cur.append(p)
                out.append(cur)
            cur = [p]
            on = not on
            k = (k + 1) % len(pattern)
            left = pattern[k]
        left -= seg - t
        if on:
            cur.append(b)
        else:
            cur = [b]
    if on and len(cur) > 1:
        out.append(cur)
    return [{"start": p[0], "segs": [("L", q) for q in p[1:]], "closed": False} for p in out]


# --- SVG frame to shapes ---------------------------------------------------------

def colour(text, opacity):
    v = int(text[1:], 16)
    return (v >> 16, (v >> 8) & 255, v & 255, round(255 * max(0.0, min(1.0, opacity))))


def bbox(contours):
    pts = [p for c in contours for p in flatten(c)]
    return min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)


def gradient(name, opacity, box):
    g = style_defs.GRADIENTS[name]
    x0, y0, x1, y1 = box
    stops = [(round(o * 255), colour(c, a * opacity)) for o, c, a in g[1]]
    if g[0] == "radial":
        return ("radial", stops, (x0 + (x1 - x0) * g[2], y0 + (y1 - y0) * g[3]), max(x1 - x0, y1 - y0) / 2)
    return ("linear", stops, (x0, y0), (x1, y1))


def shapes_of(svg):
    """[("fill", fill, contours) | ("line", width, rgba, contours)] in paint order."""
    root = ET.fromstring(f'<svg xmlns="http://www.w3.org/2000/svg">{svg}</svg>')
    out = []

    def walk(el, inherited, m):
        attrs = dict(inherited)
        attrs.update({k: v for k, v in el.attrib.items() if k != "transform"})
        m = mat_mul(m, parse_transform(el.attrib.get("transform")))
        tag = el.tag.split("}")[1]
        if tag in ("svg", "g"):
            for child in el:
                walk(child, {k: v for k, v in attrs.items() if k in ("fill", "fill-opacity", "opacity", "stroke", "stroke-width", "stroke-opacity")}, m)
            return
        f = lambda k, d=0.0: float(el.attrib.get(k, d))  # noqa: E731
        if tag == "path":
            contours = parse_d(el.attrib["d"])
        elif tag == "circle":
            contours = circle_contours(f("cx"), f("cy"), f("r"), f("r"))
        elif tag == "ellipse":
            contours = circle_contours(f("cx"), f("cy"), f("rx"), f("ry"))
        elif tag == "rect":
            contours = rect_contours(f("x"), f("y"), f("width"), f("height"), f("rx"))
        elif tag == "line":
            contours = [{"start": (f("x1"), f("y1")), "segs": [("L", (f("x2"), f("y2")))], "closed": False}]
        else:
            sys.exit("unsupported element " + tag)
        contours = transform_contours(m, contours)
        scale = math.sqrt(abs(m[0] * m[3] - m[1] * m[2]))
        opacity = float(attrs.get("opacity", 1))
        fill = attrs.get("fill", "#000000")
        if fill != "none" and tag != "line":
            closed = [dict(c, closed=True) for c in contours]
            alpha = opacity * float(attrs.get("fill-opacity", 1))
            g = re.fullmatch(r"url\(#(\w+)\)", fill)
            out.append(("fill", gradient(g.group(1), alpha, bbox(closed)) if g else ("solid", colour(fill, alpha)), closed))
        stroke = attrs.get("stroke", "none")
        if stroke != "none":
            width = float(attrs.get("stroke-width", 1)) * scale
            alpha = opacity * float(attrs.get("stroke-opacity", 1))
            g = re.fullmatch(r"url\(#(\w+)\)", stroke)
            if g:
                # SWF lines can't take a gradient: a stroked circle becomes a filled band.
                if tag != "circle":
                    sys.exit("gradient strokes only on circles")
                cx, cy, r = f("cx"), f("cy"), f("r")
                band = circle_contours(cx, cy, r + width / 2, r + width / 2) + circle_contours(cx, cy, r - width / 2, r - width / 2)
                band = transform_contours(m, band)
                out.append(("fill", gradient(g.group(1), alpha, bbox(band)), band))
            else:
                if "stroke-dasharray" in el.attrib:
                    pattern = [float(v) for v in re.split(r"[\s,]+", el.attrib["stroke-dasharray"].strip())]
                    contours = [d for c in contours for d in dashes(flatten(c, 16), pattern)]
                out.append(("line", width, colour(stroke, alpha), contours))
    root_m = IDENTITY
    for child in root:
        walk(child, {}, root_m)
    return out


# --- SWF writing -------------------------------------------------------------------

def matrix_bytes(sx, r0, r1, sy, tx, ty):
    m = bi.Bits()
    fx = [round(v * 65536) for v in (sx, sy, r0, r1)]
    m.u(1, 1)
    n = bi.sbits(fx[0], fx[1])
    m.u(n, 5)
    m.s(fx[0], n)
    m.s(fx[1], n)
    m.u(1, 1)
    n = bi.sbits(fx[2], fx[3])
    m.u(n, 5)
    m.s(fx[2], n)
    m.s(fx[3], n)
    n = bi.sbits(tx, ty)
    m.u(n, 5)
    m.s(tx, n)
    m.s(ty, n)
    return m.bytes()


def fill_style(fill):
    if fill[0] == "solid":
        return bytes([0x00]) + bytes(fill[1])
    if fill[0] == "radial":
        return bi.fill_style(fill)
    _, stops, p0, p1 = fill
    length = math.dist(p0, p1) * TWIPS
    a = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    s = length / 32768
    mid = bi.tw(((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2))
    grad = bytes([len(stops)])
    for ratio, rgba in stops:
        grad += bytes([ratio]) + bytes(rgba)
    return bytes([0x10]) + matrix_bytes(s * math.cos(a), s * math.sin(a), -s * math.sin(a), s * math.cos(a), mid[0], mid[1]) + grad


def edges(b, c, pos, xs, ys):
    for seg in c["segs"]:
        end = bi.tw(seg[-1])
        if seg[0] == "L":
            dx, dy = end[0] - pos[0], end[1] - pos[1]
            if dx == 0 and dy == 0:
                continue
            n = max(2, bi.sbits(dx, dy))
            b.u(1, 1)
            b.u(1, 1)
            b.u(n - 2, 4)
            b.u(1, 1)
            b.s(dx, n)
            b.s(dy, n)
        else:
            ctl = bi.tw(seg[1])
            cdx, cdy, adx, ady = ctl[0] - pos[0], ctl[1] - pos[1], end[0] - ctl[0], end[1] - ctl[1]
            if cdx == cdy == adx == ady == 0:
                continue
            n = max(2, bi.sbits(cdx, cdy, adx, ady))
            b.u(1, 1)
            b.u(0, 1)
            b.u(n - 2, 4)
            for v in (cdx, cdy, adx, ady):
                b.s(v, n)
            xs.append(ctl[0])
            ys.append(ctl[1])
        if n > 17:
            sys.exit("edge too long")
        pos = end
        xs.append(end[0])
        ys.append(end[1])
    return pos


def define_shape(shape_id, item):
    b = bi.Bits()
    b.u(1, 4)
    b.u(1, 4)
    xs, ys = [], []
    first = True
    pos = None
    if item[0] == "fill":
        contours = bi.orient([dict(start=c["start"], segs=c["segs"]) for c in item[2]])
        for c in contours:
            segs = list(c["segs"])
            if not segs or bi.tw(segs[-1][-1]) != bi.tw(c["start"]):
                segs.append(("L", c["start"]))
            c = {"start": c["start"], "segs": segs}
            sx, sy = bi.tw(c["start"])
            b.u(0, 1); b.u(0, 1); b.u(0, 1); b.u(1 if first else 0, 1); b.u(0, 1); b.u(1, 1)
            n = bi.sbits(sx, sy)
            b.u(n, 5); b.s(sx, n); b.s(sy, n)
            if first:
                b.u(1, 1)
                first = False
            xs.append(sx); ys.append(sy)
            pos = edges(b, c, (sx, sy), xs, ys)
        styles = bytes([1]) + fill_style(item[1]) + bytes([0])
        pad = 0
    else:
        width, rgba, contours = item[1], item[2], item[3]
        for c in contours:
            segs = list(c["segs"])
            if c["closed"] and (not segs or bi.tw(segs[-1][-1]) != bi.tw(c["start"])):
                segs.append(("L", c["start"]))
            c = {"start": c["start"], "segs": segs}
            sx, sy = bi.tw(c["start"])
            b.u(0, 1); b.u(0, 1); b.u(1 if first else 0, 1); b.u(0, 1); b.u(0, 1); b.u(1, 1)
            n = bi.sbits(sx, sy)
            b.u(n, 5); b.s(sx, n); b.s(sy, n)
            if first:
                b.u(1, 1)
                first = False
            xs.append(sx); ys.append(sy)
            pos = edges(b, c, (sx, sy), xs, ys)
        styles = bytes([0]) + bytes([1]) + struct.pack("<H", max(1, round(width * TWIPS))) + bytes(rgba)
        pad = round(width * TWIPS / 2)
    b.u(0, 6)
    bounds = (min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad)
    body = struct.pack("<H", shape_id) + bi.rect(*bounds) + styles + b.bytes()
    return bi.tag(32, body), bounds


def picture_swf(items):
    tags, places, union = b"", b"", None
    for k, item in enumerate(items):
        t, bounds = define_shape(k + 1, item)
        tags += t
        places += bi.tag(26, bytes([0x02]) + struct.pack("<HH", k + 1, k + 1))
        union = bounds if union is None else (min(union[0], bounds[0]), max(union[1], bounds[1]), min(union[2], bounds[2]), max(union[3], bounds[3]))
    body = bi.rect(0, 64 * TWIPS, 0, 64 * TWIPS) + struct.pack("<HH", 30 << 8, 1) + bi.tag(69, struct.pack("<I", 0))
    body += tags + places + bi.tag(1, b"") + bi.tag(0, b"")
    return b"FWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + body, union


# --- the meter, for the dll and widget ------------------------------------------

def num(v):
    return ("%.2f" % v).rstrip("0").rstrip(".") if v != int(v) else str(int(v))


def points_text(pts):
    # Icon units: the canvas centre (32, 32) is 0, 0.
    return " ".join(f"{num(x - 32)},{num(y - 32)}" for x, y in pts)


def path_points(d, scale=1.0, steps=8):
    pts = []
    for c in parse_d(d):
        pts += flatten(c, steps)
    if scale != 1.0:
        pts = [(32 + (x - 32) * scale, 32 + (y - 32) * scale) for x, y in pts]
    # Drop points that sit on the one before (the flattener repeats joins).
    out = [pts[0]]
    for p in pts[1:]:
        if math.dist(p, out[-1]) > 0.05:
            out.append(p)
    return out


def meter_keys(m):
    """sMeter, fWidth, sGeometry, sBy, iAlpha, and its extent on the canvas."""
    kind = m["kind"]
    if kind == "ring":
        geo = f"{num(m['cx'] - 32)},{num(m['cy'] - 32)},{num(m['r'])},{num(m['start'])}"
        ext = (m["cx"] - m["r"] - m["w"] / 2, m["cy"] - m["r"] - m["w"] / 2, m["cx"] + m["r"] + m["w"] / 2, m["cy"] + m["r"] + m["w"] / 2)
        return {"sMeter": "ring", "fWidth": num(m["w"]), "sGeometry": geo}, ext
    if kind == "trace":
        pts = path_points(m["d"], m["scale"], 12)
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return {"sMeter": "trace", "fWidth": num(m["w"]), "sGeometry": points_text(pts)}, (min(xs), min(ys), max(xs), max(ys))
    if kind == "bar":
        geo = f"{num(m['x0'] - 32)},{num(m['y'] - 32)},{num(m['x1'] - m['x0'])},{num(m['h'])}"
        return {"sMeter": "bar", "fWidth": num(m["h"]), "sGeometry": geo, "sBy": m["from"]}, (m["x0"], m["y"], m["x1"], m["y"] + m["h"])
    if kind == "steps":
        parts = [path_points(d, 1.0, 12) for d in m["items"]]
        pts = [p for part in parts for p in part]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return ({"sMeter": "steps", "fWidth": num(m["w"]), "sGeometry": "|".join(points_text(p) for p in parts), "sBy": m["by"]},
                (min(xs), min(ys), max(xs), max(ys)))
    if kind == "level":
        if m["shapes"]:
            parts = [flatten(c, 6) for _, _, cs in [s for s in shapes_of(m["shapes"]) if s[0] == "fill"] for c in cs]
        else:
            parts = [path_points(m["d"], 1.0, 8)]
        pts = [p for part in parts for p in part]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return ({"sMeter": "level", "sGeometry": "|".join(points_text(p) for p in parts), "iAlpha": str(m["alpha"])},
                (min(xs), min(ys), max(xs), max(ys)))
    return {"sMeter": kind}, None


def style_ini(n, s, frame_bounds):
    keys, ext = meter_keys(s["meter"])
    art = s["size"] / 52 * 64  # glyph pictures fit their longer side to 52 of the 64 canvas
    top, bottom = s["cy"] - s["size"] / 2, s["cy"] + s["size"] / 2
    for box in [b for b in (frame_bounds, ext) if b]:
        top, bottom = min(top, box[1]), max(bottom, box[3])
    lines = ["; Needs Widget AIO icon style. Made by build_styles.py from swf/styles/style_defs.py: edit that, not this.",
             "; The format is in docs/STYLES.md.", "[Style]",
             f"sName = {s['name']}", f"sNote = Suits {s['suits']}. {s['note']}",
             f"sFrame = {'frame_' + s['key'] if s['frame'] else ''}"]
    lines += [f"{k} = {v}" for k, v in keys.items()]
    lines += [f"fArtSize = {num(art)}", f"fArtY = {num(s['cy'] - 32)}",
              f"sGlyph = {'stage' if s['glyph'] == 'stage' else s['glyph'][1:].upper()}",
              "sColors = " + ",".join(c[1:].upper() for c in s["stages"]),
              f"bOwnColors = {1 if s['own'] else 0}", f"bTintFrame = {1 if s['tint_frame'] else 0}",
              f"fTop = {num(top - 32)}", f"fBottom = {num(bottom - 32)}"]
    return "\n".join(lines) + "\n"


# --- check sheet -------------------------------------------------------------------

def svg_of(items):
    out = []
    for item in items:
        d = ""
        for c in item[2 if item[0] == "fill" else 3]:
            d += "M%.2f %.2f " % c["start"]
            for s in c["segs"]:
                d += ("L%.2f %.2f " % s[1]) if s[0] == "L" else ("Q%.2f %.2f %.2f %.2f " % (s[1] + s[2]))
            if item[0] == "fill" or c["closed"]:
                d += "Z "
        if item[0] == "fill":
            f = item[1]
            if f[0] == "solid":
                r, g, b, a = f[1]
                paint = f'fill="#{r:02x}{g:02x}{b:02x}" fill-opacity="{a / 255:.3f}"'
            else:
                r, g, b, a = f[1][-1][1]
                paint = f'fill="#{r:02x}{g:02x}{b:02x}" fill-opacity="{a / 255:.3f}"'
            out.append(f'<path d="{d}" {paint} fill-rule="evenodd"/>')
        else:
            r, g, b, a = item[2]
            out.append(f'<path d="{d}" fill="none" stroke="#{r:02x}{g:02x}{b:02x}" stroke-opacity="{a / 255:.3f}" stroke-width="{item[1]:.2f}" stroke-linejoin="round" stroke-linecap="round"/>')
    return "".join(out)


def meter_svg(ini, share, stage, colour):
    k = dict(re.findall(r"^(\w+) = (.*)$", ini, re.M))
    kind, w = k["sMeter"], float(k.get("fWidth", 0) or 0)
    tr = 'transform="translate(32 32)"'
    if kind == "ring":
        cx, cy, r, start = map(float, k["sGeometry"].split(","))
        a0, a1 = math.radians(start - 90), math.radians(start - 90 + 359.9 * share)
        big = 1 if share > .5 else 0
        return (f'<path {tr} d="M{cx + r * math.cos(a0):.2f} {cy + r * math.sin(a0):.2f} A{r} {r} 0 {big} 1 {cx + r * math.cos(a1):.2f} {cy + r * math.sin(a1):.2f}" '
                f'fill="none" stroke="{colour}" stroke-width="{w}"/>')
    if kind == "trace":
        pts = [tuple(map(float, p.split(","))) for p in k["sGeometry"].split()]
        total = sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))
        left, keep = total * share, [pts[0]]
        for a, b in zip(pts, pts[1:]):
            seg = math.dist(a, b)
            if seg >= left:
                t = left / seg if seg else 0
                keep.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
                break
            keep.append(b)
            left -= seg
        return f'<polyline {tr} points="{" ".join("%.2f,%.2f" % p for p in keep)}" fill="none" stroke="{colour}" stroke-width="{w}" stroke-linejoin="round"/>'
    if kind == "bar":
        x, y, length, h = map(float, k["sGeometry"].split(","))
        if k["sBy"] == "centre":
            x, length = x + length * (1 - share) / 2, length * share
        else:
            length *= share
        return f'<rect {tr} x="{x}" y="{y}" width="{length:.2f}" height="{h}" fill="{colour}"/>'
    if kind == "steps":
        parts = k["sGeometry"].split("|")
        lit = math.ceil(stage * len(parts) / 5) if k["sBy"] == "stage" else round(share * len(parts))
        out = ""
        for p in parts[:lit]:
            pts = " ".join("%s" % q for q in p.split())
            out += (f'<polyline {tr} points="{pts}" fill="none" stroke="{colour}" stroke-width="{w}"/>' if w > 0
                    else f'<polygon {tr} points="{pts}" fill="{colour}"/>')
        return out
    if kind == "level":
        parts = k["sGeometry"].split("|")
        ys = [float(q.split(",")[1]) for p in parts for q in p.split()]
        top, bottom = min(ys), max(ys)
        lv = bottom - (bottom - top) * share
        cid = "c%d" % abs(hash((ini, share)))
        return (f'<clipPath id="{cid}">' + "".join(f'<polygon {tr} points="{p}"/>' for p in parts) + "</clipPath>"
                f'<rect x="0" y="{lv + 32:.2f}" width="64" height="64" fill="{colour}" fill-opacity="{int(k["iAlpha"]) / 100}" clip-path="url(#{cid})"/>')
    return ""


def main():
    os.makedirs(OUT_ICONS, exist_ok=True)
    os.makedirs(OUT_INI, exist_ok=True)
    sheet = []
    for n, s in enumerate(style_defs.STYLES, 2):
        items = shapes_of(s["frame"]) if s["frame"] else []
        frame_bounds = None
        if items:
            data, (x0, x1, y0, y1) = picture_swf(items)
            frame_bounds = (x0 / TWIPS, y0 / TWIPS, x1 / TWIPS, y1 / TWIPS)
            with open(os.path.join(OUT_ICONS, f"frame_{s['key']}.swf"), "wb") as f:
                f.write(data)
            print("style %-15s frame %6d bytes sha256 %s" % (s["key"], len(data), hashlib.sha256(data).hexdigest()[:16]))
        ini = style_ini(n, s, frame_bounds)
        with open(os.path.join(OUT_INI, s["key"] + ".ini"), "w", encoding="utf-8", newline="\r\n") as f:
            f.write(ini)
        if "--sheet" in sys.argv:
            row = []
            for share, stage in ((0.0, 0), (.35, 1), (.7, 3), (1.0, 5)):
                c = s["stages"][stage]
                frame = svg_of(items)
                if s["tint_frame"]:
                    frame = f'<g style="filter:url(#t{stage}{n})">{frame}</g>'
                    frame = frame.replace('fill="#ffffff"', f'fill="{c}"')
                glyph_c = c if s["glyph"] == "stage" else s["glyph"]
                gy, gs = s["cy"], s["size"]
                row.append(f'<svg viewBox="0 0 64 64">{frame}{meter_svg(ini, share, stage, c)}'
                           f'<rect x="{32 - gs / 2}" y="{gy - gs / 2}" width="{gs}" height="{gs}" fill="none" stroke="{glyph_c}" stroke-dasharray="2 2"/></svg>')
            sheet.append(f'<div class="l"><b>{n}. {s["name"]}</b>{"".join(row)}</div>')
    if "--sheet" in sys.argv:
        html = ('<!doctype html><meta charset="utf-8"><style>body{background:#2b2d30;color:#ddd;font:12px sans-serif;margin:8px;'
                'display:grid;grid-template-columns:repeat(2,1fr);gap:2px}.l{display:flex;align-items:center;gap:4px}b{width:130px}'
                'svg{width:56px;height:56px;background:#555a50}</style>' + "".join(sheet))
        with open(os.path.join(HERE, "..", "build", "styles-check.html"), "w", encoding="utf-8") as f:
            f.write(html)
    print("styles ok:", len(style_defs.STYLES))


if __name__ == "__main__":
    main()
