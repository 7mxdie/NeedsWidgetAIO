"""Check every SKSE Menu Framework function NeedsWidget.dll calls exists in an SMF dll.

    python check_smf_exports.py [SKSEMenuFramework.dll ...]

The SMF header looks its functions up by name (GetProcAddress) every time they
are called, so a name missing from the installed SMF crashes the game the moment
the settings page draws. The names the dll can ask for are the header's string
literals that made it into NeedsWidget.dll; each one must be exported by every
SMF dll given (default: the one live in Tom's game).
"""

import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(HERE, "lib", "SKSEMenuFramework", "SKSEMenuFramework.h")
DLL = os.path.join(HERE, "build", "windows", "x64", "release", "NeedsWidget.dll")
LIVE = r"G:\Games\SteamLibrary\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\SKSEMenuFramework.dll"


def exports(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    sections = struct.unpack_from("<H", d, pe + 6)[0]
    optional = pe + 24
    table = optional + struct.unpack_from("<H", d, pe + 20)[0]
    export_rva = struct.unpack_from("<I", d, optional + 112)[0]

    def offset(rva):
        for k in range(sections):
            size, va = struct.unpack_from("<II", d, table + 40 * k + 8)
            raw = struct.unpack_from("<I", d, table + 40 * k + 20)[0]
            if va <= rva < va + max(size, 1):
                return rva - va + raw
        raise ValueError("rva %x outside every section" % rva)

    e = offset(export_rva)
    count = struct.unpack_from("<I", d, e + 24)[0]
    names = offset(struct.unpack_from("<I", d, e + 32)[0])
    out = set()
    for k in range(count):
        s = offset(struct.unpack_from("<I", d, names + 4 * k)[0])
        out.add(d[s:d.index(b"\0", s)].decode("ascii"))
    return out


def main():
    header = open(HEADER, encoding="utf-8", errors="replace").read()
    ours = open(DLL, "rb").read()
    candidates = set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"', header))
    used = sorted(n for n in candidates if b"\0" + n.encode() + b"\0" in ours)
    targets = sys.argv[1:] or [LIVE]
    missing = 0
    for target in targets:
        have = exports(target)
        gaps = [n for n in used if n not in have]
        missing += len(gaps)
        print("%s: %d exports, %d of %d names the dll uses are missing%s" % (
            target, len(have), len(gaps), len(used), (": " + ", ".join(gaps)) if gaps else ""))
    print("names the dll uses: " + ", ".join(used))
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
