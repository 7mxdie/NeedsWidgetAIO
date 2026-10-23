# Needs Widget AIO

Source code for [Needs Widget AIO](https://www.nexusmods.com/skyrimspecialedition/mods/196226), an SKSE plugin that shows every needs mod's needs as icons on the Skyrim HUD.

Copyright (C) 2026 mxdie. Licensed under the GNU General Public License v3.0 (see `LICENSE.txt`), because the dll is built on [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), which is GPL-3.0.

How to build it is in `BUILDING.txt`.

- `plugin/` - the dll (C++, CommonLibSSE-NG, xmake) and the files the mod ships (`plugin/dist`)
- `swf/` - the widget (ActionScript 2) and the icon art pipeline
- `package.py` - builds and zips a release
