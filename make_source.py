"""Zip Needs Widget AIO's source code, for the GPL (Nexus moderation, 2026-10-23).

    python make_source.py 2.3.0
    python make_source.py 2.3.0 --to <folder>   (copy into a folder instead: the GitHub repo)

The dll links CommonLibSSE-NG, which is GPL-3.0, so the dll is GPL-3.0 too and its
source has to be available. This writes Release/<label>/NeedsWidgetAIO-<label>-source.zip:
the dll's source and build file, the widget's ActionScript, the art and build
scripts, the files the mod ships, the GPL text and BUILDING.txt. CommonLibSSE-NG
itself is unmodified upstream, so BUILDING.txt names the exact commit instead of
copying it in. Then it reopens the zip and checks every entry against its file.
"""

import hashlib
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP_DIRS = {"__pycache__", "build", ".xmake", ".git", ".vs"}
TREES = [
    "plugin/src",
    "plugin/dist",
    "plugin/lib/SKSEMenuFramework",
    "swf/src",
    "swf/art",
    "swf/tests",
]
FILES = [
    "package.py",
    "make_source.py",
    "plugin/xmake.lua",
    "plugin/check_smf_exports.py",
    "swf/build_widget.py",
    "swf/build_icons.py",
]
COMMONLIB = "https://github.com/alandtse/CommonLibSSE-NG"
COMMONLIB_COMMIT = "12d61f3c39f7d4ba1b1ab543df2bca0e613dc747"  # 13.1.0

BUILDING = """Needs Widget AIO {label} - source code
Copyright (C) 2026 mxdie

Licensed under the GNU General Public License v3.0, see LICENSE.txt.

The dll
-------
1. Put CommonLibSSE-NG 13.1.0, unmodified, in plugin/lib/commonlibsse-ng:
     git clone {commonlib} plugin/lib/commonlibsse-ng
     git -C plugin/lib/commonlibsse-ng checkout {commit}
2. In plugin/, run: xmake build
   (xmake and Visual Studio 2022 with C++). The dll lands in plugin/build/.

The widget and pictures
-----------------------
In swf/, run: python build_widget.py
It needs JPEXS FFDec (ffdec-cli) to compile src/widget.as, and Ruffle desktop
for the --probe check. Paths to both are at the top of build_widget.py.

package.py does all of it and zips the mod the way it's released.

plugin/lib/SKSEMenuFramework is the SKSE Menu Framework header, MIT License
(its LICENSE is next to it). The icons in swf/art/game-icons are from
game-icons.net under CC BY 3.0, see the README in plugin/dist.
"""


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if len(sys.argv) not in (2, 4) or (len(sys.argv) == 4 and sys.argv[2] != "--to"):
        sys.exit(__doc__)
    label = sys.argv[1]
    files = {}
    for tree in TREES:
        for root, dirs, names in os.walk(os.path.join(HERE, tree)):
            dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
            for name in sorted(names):
                path = os.path.join(root, name)
                files[os.path.relpath(path, HERE).replace(os.sep, "/")] = path
    for f in FILES:
        files[f] = os.path.join(HERE, f)
    files["LICENSE.txt"] = os.path.join(HERE, "plugin", "dist", "SKSE", "Plugins", "NeedsWidget", "LICENSE.txt")
    for arc, src in files.items():
        if not os.path.isfile(src):
            sys.exit("missing: %s" % src)
    building = BUILDING.format(label=label, commonlib=COMMONLIB, commit=COMMONLIB_COMMIT).replace("\n", "\r\n").encode("utf-8")

    if len(sys.argv) == 4:
        # Copies over what's there; a file deleted from the source has to go by hand.
        to = os.path.abspath(sys.argv[3])
        for arc, src in files.items():
            dest = os.path.join(to, arc)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(src, "rb") as f, open(dest, "wb") as g:
                g.write(f.read())
        with open(os.path.join(to, "BUILDING.txt"), "wb") as g:
            g.write(building)
        print("%d files copied to %s" % (len(files) + 1, to))
        return

    out_dir = os.path.join(HERE, "Release", label)
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "NeedsWidgetAIO-%s-source.zip" % label)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("BUILDING.txt", building)
        for arc, src in files.items():
            z.write(src, arc)

    with zipfile.ZipFile(out) as z:
        if sorted(z.namelist()) != sorted(list(files) + ["BUILDING.txt"]):
            sys.exit("zip contents wrong")
        for arc, src in files.items():
            if sha256(z.read(arc)) != sha256(open(src, "rb").read()):
                sys.exit("zip entry differs from source: %s" % arc)
    data = open(out, "rb").read()
    print("%s  %d files  %d bytes  sha256 %s" % (out, len(files) + 1, len(data), sha256(data)))


if __name__ == "__main__":
    main()
