"""Build Interface/NeedsWidget/widget.swf from src/widget.as, plus the pictures.

1. Build the pictures in icons/ (build_icons.py).
2. Write a minimal SWF 8 file: 1280x720, 30 fps, one frame holding an empty
   DoAction tag.
3. Have JPEXS FFDec compile src/widget.as into that DoAction (-replace).
4. Decompile the result and check nothing went missing on the way: the same
   functions, each using the same names, numbers and strings. The decompiler
   writes loops and conditions back in its own form, so layout is not compared;
   the probe below checks behaviour.
5. With --probe, run tests/probe_widget.as in Ruffle and check the widget really
   loads its pictures, draws and lays out. This caught FFDec not hoisting
   functions: a call above a function's definition silently did nothing and the
   TEST1 icon drew blank.

Requires JPEXS FFDec (ffdec-cli.exe); --probe also needs Ruffle desktop.
"""

import collections
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

import build_icons

FFDEC = r"C:\Program Files (x86)\FFDec\ffdec-cli.exe"
RUFFLE = r"C:\Users\mxdie\Documents\AI\Tools\ruffle\ruffle.exe"
HERE = os.path.dirname(os.path.abspath(__file__))
PROBE_SRC = os.path.join(HERE, "tests", "probe_widget.as")
SRC = os.path.join(HERE, "src", "widget.as")
OUT_DIR = os.path.join(HERE, "build", "Interface", "NeedsWidget")
OUT = os.path.join(OUT_DIR, "widget.swf")
SCRIPT_PATH = "\\frame_1\\DoAction"


def probe_expect(bounds):
    # What the probe must print. Tints use colours whose percentages are whole
    # numbers, so storage rounding can't move them.
    return [
        "NWPROBE start shown=false icons=34 ext=true",
        "NWPROBE art 0 icons/ bounds=" + bounds["hunger"],
        "NWPROBE art 1 icons/ bounds=" + bounds["cold"],
        "NWPROBE art 2 icons/ bounds=" + bounds["fatigue"],
        "NWPROBE art 3 icons/ bounds=" + bounds["stress"],
        "NWPROBE art 4 NeedsWidget/icons/ bounds=" + bounds["dirt"],
        "NWPROBE art 5 fallback bounds=-9,-9,9,9",
        "NWPROBE art 6 icons/ bounds=" + bounds["thirst"],
        "NWPROBE art 7 icons/ bounds=" + bounds["wetness"],
        "NWPROBE art 8 icons/ bounds=" + bounds["vitality"],
        "NWPROBE art 9 icons/ bounds=" + bounds["disease"],
    ] + ["NWPROBE art %d none" % k for k in range(10, 34)] + [
        "NWPROBE frame ring 34 icons/ bounds=" + bounds["frame_ring"] + " x=-32 y=-32",
        "NWPROBE layout x=640 y=360 xs=150 a=80 label=true labelY=31",
        "NWPROBE place xs=56.25 x=-18 y=-18",
        "NWPROBE half visible=true text=50% meter=0,-28.4,28.4,28.4 tint=100,40,20",
        "NWPROBE quarter meter=0,-28.4,28.4,0 tint=80,20,0",
        "NWPROBE pulse dips=true peaks=true meterInStep=true frameStill=true",
        "NWPROBE empty meterW=0 tint=100,100,100 a=100",
        "NWPROBE frame bar 68 icons/ bounds=" + bounds["frame_bar"],
        "NWPROBE bar meter=-16.2,21.8,0,22.6 place xs=59.375 x=-19 y=-23 labelY=25",
        "NWPROBE nopulse min=100",
        "NWPROBE icon frames=102 frame=none frameW=0 meterW=0 tint=100,100,100",
        "NWPROBE numbers x=-230 w=480 size=32 color=ff9966",
        "NWPROBE edge x=-232 dy=-2 text=75% color=0 on=true",
        "NWPROBE edgeoff on=false",
        "NWPROBE alignleft x=40 align=left edge=left",
        "NWPROBE alignright x=-280 align=right",
        "NWPROBE plain x=-120 w=240 size=16 color=ffffff",
        "NWPROBE corner x=0 y=720",
        "NWPROBE offset x=588.5 y=385",
        "NWPROBE hidden icon=false shown=false",
        "NWPROBE fade out a=0",
        "NWPROBE fade in a=100",
        "NWPROBE flash bright=true",
        "NWPROBE flash over rb=0",
        "NWPROBE extra art 10 icons/ bounds=" + bounds["cold"],
        "NWPROBE extra again loads=1",
        "NWPROBE done",
    ]


def tag(code, body):
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def rect(xmin, xmax, ymin, ymax, nbits=16):
    bits = format(nbits, "05b")
    for v in (xmin, xmax, ymin, ymax):
        bits += format(v & ((1 << nbits) - 1), "0%db" % nbits)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


def base_swf():
    body = rect(0, 1280 * 20, 0, 720 * 20)
    body += struct.pack("<HH", 30 << 8, 1)          # 30 fps, 1 frame
    body += tag(69, struct.pack("<I", 0))           # FileAttributes: AS2, no network
    body += tag(9, b"\x00\x00\x00")                 # SetBackgroundColor
    body += tag(12, b"\x00")                        # DoAction: ActionEnd only
    body += tag(1, b"")                             # ShowFrame
    body += tag(0, b"")                             # End
    return b"FWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + body


def ffdec(*args):
    r = subprocess.run([FFDEC] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ffdec failed:\n" + r.stdout + r.stderr)
    return r.stdout


TOKEN = re.compile(r'"[^"]*"|0x[0-9A-Fa-f]+|\d+(?:\.\d+)?|[A-Za-z_$][A-Za-z0-9_$]*|[^\sA-Za-z0-9_$"]')
# Words the decompiler may add, drop or move when it rebuilds loops and conditions.
STRUCTURE = {"var", "for", "while", "do", "if", "else", "break", "continue", "function", "return"}


def normalise(text):
    """{function name: Counter of names, numbers and strings}, plus the same for frame code.

    Locals all count as "L": the decompiler renames them (_loc3_) and may declare
    them in a different place. Hex is compared as decimal, X["n"] as X.n.
    """
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'\[\s*"([A-Za-z_$][A-Za-z0-9_$]*)"\s*\]', r".\1", text)
    # Forms the decompiler writes differently but means the same: it hoists bare
    # declarations out of loops, writes i++ on a local as i = i + 1, and says
    # return undefined for return.
    text = re.sub(r"\bvar\s+[A-Za-z_$][\w$]*\s*;", "", text)
    text = re.sub(r"([A-Za-z_$][\w$.]*)\s*\+\+", r"\1 = \1 + 1", text)
    text = re.sub(r"([A-Za-z_$][\w$.]*)\s*--", r"\1 = \1 - 1", text)
    text = re.sub(r"\breturn\s+undefined\b", "return", text)
    toks = [str(int(t, 16)) if t.lower().startswith("0x") else t for t in TOKEN.findall(text)]

    def is_local(body, n, locals_):
        # A name right after a dot is a property, and a name between { or , and
        # a colon is an object key: neither is the local of the same name.
        if body[n] not in locals_:
            return False
        before = body[n - 1] if n > 0 else ""
        after = body[n + 1] if n + 1 < len(body) else ""
        return before != "." and not (before in ("{", ",") and after == ":")

    def words(body, locals_):
        return collections.Counter(
            "L" if is_local(body, n, locals_) else t
            for n, t in enumerate(body)
            if t not in STRUCTURE and (t[0].isalnum() or t[0] in '_$"'))

    functions, frame, i = {}, [], 0
    while i < len(toks):
        if toks[i] == "function" and i + 1 < len(toks) and toks[i + 1] != "(":
            name = toks[i + 1]
            j = toks.index("{", i)
            depth, k = 0, j
            while True:
                if toks[k] == "{":
                    depth += 1
                elif toks[k] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            body = toks[i + 2:k + 1]
            locals_ = {body[n + 1] for n in range(len(body) - 1) if body[n] == "var"}
            locals_ |= {t for t in body if re.fullmatch(r"_loc\d+_", t)}
            functions[name] = words(body, locals_)
            i = k + 1
        else:
            frame.append(toks[i])
            i += 1
    return functions, words(frame, set())


def round_trip(source, built):
    want_fn, want_frame = normalise(source)
    got_fn, got_frame = normalise(built)
    if sorted(want_fn) != sorted(got_fn):
        sys.exit("function set differs: source %s, built %s" % (sorted(want_fn), sorted(got_fn)))
    for name in want_fn:
        if want_fn[name] != got_fn[name]:
            sys.exit("function %s differs:\n  only in source %s\n  only in built  %s" % (
                name, dict(want_fn[name] - got_fn[name]), dict(got_fn[name] - want_fn[name])))
    if want_frame != got_frame:
        sys.exit("frame code differs:\n  only in source %s\n  only in built  %s" % (
            dict(want_frame - got_frame), dict(got_frame - want_frame)))
    return len(want_fn), sum(want_frame.values())


def compile_as(src, out, work):
    base = os.path.join(work, "base_%s.swf" % os.path.basename(out))
    with open(base, "wb") as f:
        f.write(base_swf())
    ffdec("-replace", base, out, SCRIPT_PATH, src)


def main():
    bounds = build_icons.build()
    work = tempfile.mkdtemp(prefix="nw_swf_")
    try:
        os.makedirs(OUT_DIR, exist_ok=True)
        compile_as(SRC, OUT, work)

        export_dir = os.path.join(work, "export")
        ffdec("-export", "script", export_dir, OUT)
        found = []
        for root, _, files in os.walk(export_dir):
            for name in files:
                if name.endswith(".as"):
                    found.append(os.path.join(root, name))
        if len(found) != 1:
            sys.exit("expected exactly one script in the built SWF, found %d" % len(found))
        with open(found[0], encoding="utf-8") as f:
            built = f.read()
        with open(SRC, encoding="utf-8") as f:
            source = f.read()
        functions, words = round_trip(source, built)

        h = hashlib.sha256(open(OUT, "rb").read()).hexdigest()
        print("built %s (%d bytes) sha256 %s" % (OUT, os.path.getsize(OUT), h))
        print("round-trip ok: %d functions, %d frame-level words" % (functions, words))

        if "--probe" in sys.argv:
            probe(work, bounds)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def probe(work, bounds):
    stage = os.path.join(work, "probe")
    icons = os.path.join(build_icons.OUT_DIR)
    for sub in ("icons", os.path.join("NeedsWidget", "icons")):
        os.makedirs(os.path.join(stage, sub), exist_ok=True)
    shutil.copy(OUT, os.path.join(stage, "widget.swf"))
    for name in ("hunger", "cold", "fatigue", "stress", "thirst", "wetness", "vitality", "disease", "frame_ring", "frame_bar"):
        shutil.copy(os.path.join(icons, name + ".swf"), os.path.join(stage, "icons"))
    shutil.copy(os.path.join(icons, "dirt.swf"), os.path.join(stage, "NeedsWidget", "icons"))
    # encumbrance.swf is left out on purpose: the widget must fall back to a dot.

    probe_swf = os.path.join(stage, "probe.swf")
    compile_as(PROBE_SRC, probe_swf, work)
    try:
        r = subprocess.run([RUFFLE, "--width", "320", "--height", "180", probe_swf],
                           capture_output=True, text=True, timeout=15, cwd=stage)
        out = r.stdout + r.stderr
    except subprocess.TimeoutExpired as e:
        # Ruffle keeps running until closed, so the timeout is the normal exit.
        def text(v):
            return v.decode("utf-8", "replace") if isinstance(v, bytes) else (v or "")
        out = text(e.stdout) + text(e.stderr)
    got = [re.sub(r"\x1b\[[0-9;]*m", "", line).split("avm_trace: ", 1)[-1].strip()
           for line in out.splitlines() if "NWPROBE" in line]
    want = probe_expect(bounds)
    if got != want:
        lines = ["probe mismatch:"]
        for k in range(max(len(got), len(want))):
            g = got[k] if k < len(got) else "<missing>"
            e = want[k] if k < len(want) else "<extra>"
            lines.append(("   " if g == e else "!! ") + "got  " + g)
            if g != e:
                lines.append("   want " + e)
        sys.exit("\n".join(lines))
    # Ruffle has no visibleRect, so the widget must say it used the stage size,
    # and say it once: setLayout runs on every slider move.
    area = [re.sub(r"\x1b\[[0-9;]*m", "", line).split("avm_trace: ", 1)[-1].strip()
            for line in out.splitlines() if "NeedsWidget: screen area" in line]
    if area != ["NeedsWidget: screen area 0,0 1280x720 from stage size"]:
        sys.exit("screen area log wrong: %s" % area)
    print("ruffle probe ok: %d checks, screen area logged once" % len(got))


if __name__ == "__main__":
    main()
