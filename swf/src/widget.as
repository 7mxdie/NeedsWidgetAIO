// Needs Widget AIO - HUD layer, frame 1 of _root.
// Ten icons (hunger, cold, fatigue, stress, dirt, encumbrance, thirst, wetness,
// vitality, disease), in the order of the dll's need ids, then eight slots for
// extra icons from addon files, whose pictures the dll names with setArt.
//
// NeedsWidget.dll drives everything through setStyle, setNumbers, setLayout,
// setIcon, setShown and setArt. The only thing that runs on its own is the pulse on an icon at its
// worst stage (pulseTick, every frame).
//
// No art lives in this file. Each icon loads two pictures from icons/: a frame
// (frame_ring.swf or frame_bar.swf, named by setStyle, or none for the Icon only
// look) and its glyph (hunger.swf, cold.swf ...). Both are plain shapes with no
// code, drawn on a 64 x 64 canvas. A reskin replaces those files and
// NeedsWidget.ini and never touches this one. Only the meter's fill is drawn
// here, because it moves.
//
// Functions come first and the setup code last. FFDec's compiler does NOT hoist
// function definitions the way Adobe's does, so calling one above its definition
// silently does nothing (found in Ruffle: the TEST1 icon drew nothing).

// Moves the pen along a circle of radius r, clockwise from angle a0 to a1
// (radians, 0 = 12 o'clock), in curve steps of 45 degrees or less.
function arcTo(mc, r, a0, a1)
{
	var steps = Math.ceil(Math.abs(a1 - a0) / (Math.PI / 4));
	if (steps < 1)
	{
		steps = 1;
	}
	var step = (a1 - a0) / steps;
	var k = r / Math.cos(step / 2);
	for (var i = 0; i < steps; i++)
	{
		var mid = a0 + step * (i + 0.5);
		var end = a0 + step * (i + 1);
		mc.curveTo(Math.sin(mid) * k, -Math.cos(mid) * k, Math.sin(end) * r, -Math.cos(end) * r);
	}
}

// A band between two radii, from angle a0 to a1.
function drawBand(mc, rOut, rIn, a0, a1, color, alpha)
{
	mc.beginFill(color, alpha);
	mc.moveTo(Math.sin(a0) * rOut, -Math.cos(a0) * rOut);
	arcTo(mc, rOut, a0, a1);
	mc.lineTo(Math.sin(a1) * rIn, -Math.cos(a1) * rIn);
	arcTo(mc, rIn, a1, a0);
	mc.lineTo(Math.sin(a0) * rOut, -Math.cos(a0) * rOut);
	mc.endFill();
}

function drawRect(mc, x, y, w, h, color, alpha)
{
	mc.beginFill(color, alpha);
	mc.moveTo(x, y);
	mc.lineTo(x + w, y);
	mc.lineTo(x + w, y + h);
	mc.lineTo(x, y + h);
	mc.lineTo(x, y);
	mc.endFill();
}

function drawDot(mc, r, color, alpha)
{
	mc.beginFill(color, alpha);
	mc.moveTo(0, -r);
	arcTo(mc, r, 0, Math.PI * 2);
	mc.endFill();
}

function stageColor(stage)
{
	if (stage < 0)
	{
		stage = 0;
	}
	if (stage > 5)
	{
		stage = 5;
	}
	return stageColors[stage];
}

function drawMeter(i)
{
	var o = icons[i];
	var m = o.clip.meter;
	m.clear();
	var f = o.fraction > 1 ? 1 : o.fraction;
	if (f <= 0 || style.look == "icon")
	{
		return;
	}
	var color = stageColor(o.stage);
	if (style.look == "ring")
	{
		var half = style.ringWidth / 2;
		drawBand(m, style.ringRadius + half, style.ringRadius - half, 0, Math.PI * 2 * f, color, 100);
	}
	else
	{
		drawRect(m, style.barLeft, style.barTop, style.barLength * f, style.barHeight, color, 100);
	}
}

// Multiplies the glyph's colours by the stage colour: white art comes out in
// exactly that colour, the dark glow round it stays dark.
function applyTint(i)
{
	var o = icons[i];
	var c = style.tintArt ? stageColor(o.stage) : 0xFFFFFF;
	var t = new Color(o.clip.art);
	t.setTransform({ra: (c >> 16 & 255) / 2.55, ga: (c >> 8 & 255) / 2.55, ba: (c & 255) / 2.55, aa: 100, rb: 0, gb: 0, bb: 0, ab: 0});
	o.clip.label.textColor = numberStyle.tint ? stageColor(o.stage) : 0xFFFFFF;
}

// The numbers (TEST9, Tom: bigger, moved about, and easier to see). sizePct scales
// the text from its 16-unit base, tint colours it like its icon, dx nudges it
// sideways in icon units. Its height is the dll's, through setStyle's labelY.
// edge (2.3.0, phbd01): a dark edge round the text; missing means on.
// align (2.4.0, phbd01, for vertical HUDs): 0 or missing centred on the icon, 1 the
// text starts at the icon's middle + dx, 2 it ends there. Names then line up.
// font (2.6.0, HjalmarSeavuulf): a fontconfig.txt name; missing keeps the one set below.
function setNumbers(sizePct, tint, dx, edge, align, font)
{
	if (font != undefined && font != "")
	{
		labelFormat.font = font;
	}
	numberStyle = {size: sizePct, tint: tint, dx: dx, edge: edge != false};
	var k = sizePct / 100;
	labelFormat.size = LABEL_SIZE * k;
	labelFormat.align = align == 1 ? "left" : (align == 2 ? "right" : "center");
	var left = align == 1 ? dx : (align == 2 ? dx - LABEL_W * k : LABEL_X * k + dx);
	for (var i = 0; i < ICON_COUNT; i++)
	{
		var l = icons[i].clip.label;
		l._x = left;
		l._width = LABEL_W * k;
		l._height = LABEL_H * k;
		l.setNewTextFormat(labelFormat);
		l.setTextFormat(labelFormat);
		applyTint(i);
		syncEdge(i);
	}
}

// The dark edge: four black copies of the number behind it, nudged diagonally by a
// sixteenth of the text's size, so it reads over anything. Filters would be neater,
// but plain text fields draw the same in every Scaleform build.
function syncEdge(i)
{
	var o = icons[i];
	var l = o.clip.label;
	var k = numberStyle.size / 100;
	for (var e = 0; e < 4; e++)
	{
		var t = o.clip["edge" + e];
		t._x = l._x + EDGE_X[e] * k;
		t._y = l._y + EDGE_Y[e] * k;
		t._width = l._width;
		t._height = l._height;
		t.setNewTextFormat(labelFormat);
		t.setTextFormat(labelFormat);
		t.text = l.text;
		t.textColor = 0x000000;
		t._visible = l._visible && numberStyle.edge;
	}
}

function placeArt(i)
{
	var o = icons[i];
	var art = o.clip.art;
	if (o.source == "fallback")
	{
		art._xscale = 100;
		art._yscale = 100;
		art._x = 0;
		art._y = style.artY;
		return;
	}
	art._xscale = style.artSize / ART_CANVAS * 100;
	art._yscale = art._xscale;
	art._x = -style.artSize / 2;
	art._y = style.artY - style.artSize / 2;
}

function pictureLoaded(i, kind, attempt, path)
{
	var o = icons[i];
	if (kind == "art")
	{
		o.source = ART_PATHS[attempt];
		placeArt(i);
		applyTint(i);
		artDone++;
	}
	else
	{
		o.frameSource = ART_PATHS[attempt];
		o.clip.frame._x = -ART_CANVAS / 2;
		o.clip.frame._y = -ART_CANVAS / 2;
		frameDone++;
	}
	trace("NeedsWidget: loaded " + path);
}

// Loads icons/<file>.swf into the icon's "frame" or "art" clip. How Skyrim
// resolves a relative path from this movie is not pinned down, so each of
// ART_PATHS is tried in turn; the first that loads wins and the log says which.
function loadPicture(i, kind, attempt)
{
	var o = icons[i];
	var file = kind == "frame" ? style.frame : ICON_NAMES[i];
	var target = o.clip.createEmptyMovieClip(kind, kind == "frame" ? 1 : 3);
	if (kind == "frame" && (file == undefined || file == ""))
	{
		o.frameSource = "none";
		frameDone++;
		return;
	}
	if (kind == "art" && file == "")
	{
		// An extra slot no addon has filled (yet): nothing to load, never shown.
		o.source = "none";
		artDone++;
		return;
	}
	if (attempt >= ART_PATHS.length)
	{
		trace("NeedsWidget: could not load " + file + ".swf from any path");
		if (kind == "art")
		{
			// No glyph anywhere: a plain dot, so the icon still shows.
			drawDot(target, 9, 0xFFFFFF, 100);
			o.source = "fallback";
			placeArt(i);
			applyTint(i);
			artDone++;
		}
		else
		{
			o.frameSource = "none";
			frameDone++;
		}
		return;
	}
	var path = ART_PATHS[attempt] + file + ".swf";
	var listener = {index: i, kind: kind, attempt: attempt, path: path};
	listener.onLoadInit = function(clip)
	{
		pictureLoaded(this.index, this.kind, this.attempt, this.path);
	};
	listener.onLoadError = function(clip, error)
	{
		loadPicture(this.index, this.kind, this.attempt + 1);
	};
	var loader = new MovieClipLoader();
	loader.addListener(listener);
	o[kind + "Loader"] = loader;
	loader.loadClip(path, target);
}

// An extra icon's picture, icons/<name>.swf, as named in its addon file. The
// dll calls this once per extra icon after the movie loads; the same name again
// does nothing.
function setArt(i, name)
{
	if (ICON_NAMES[i] == name)
	{
		return;
	}
	ICON_NAMES[i] = name;
	icons[i].source = undefined;
	loadPicture(i, "art", 0);
}

// look: "ring", "bar" or "icon". frame: picture name in icons/, "" for none.
// Sizes are in icon units: the icon is 64 across, centred on 0,0.
// c0-c5 are the stage colours, best to worst, already picked from the player's
// colour scheme by the dll. pulse: fade an icon at its worst stage in and out.
function setStyle(look, frame, artSize, artY, ringRadius, ringWidth, barLeft, barTop, barLength, barHeight, labelY, tintArt, c0, c1, c2, c3, c4, c5, pulse)
{
	var reload = frame != style.frame;
	style = {look: look, frame: frame, artSize: artSize, artY: artY, ringRadius: ringRadius, ringWidth: ringWidth, barLeft: barLeft, barTop: barTop, barLength: barLength, barHeight: barHeight, labelY: labelY, tintArt: tintArt, pulse: pulse};
	stageColors = [c0, c1, c2, c3, c4, c5];
	for (var i = 0; i < ICON_COUNT; i++)
	{
		if (reload)
		{
			loadPicture(i, "frame", 0);
		}
		placeArt(i);
		applyTint(i);
		drawMeter(i);
		icons[i].clip.label._y = labelY;
		syncEdge(i);
	}
}

// Positions are percentages of the visible screen, so the same numbers work on
// 16:9 and ultrawide. visibleRect is a Scaleform extension (the setup code
// switches extensions on); fall back to the stage size if it is missing. The log
// says which was used, once, and again only if it changes.
function visibleArea()
{
	var r = Stage["visibleRect"];
	var a = {x: 0, y: 0, w: BASE_W, h: BASE_H, via: "stage size"};
	if (r != undefined && r.width > 0)
	{
		a = {x: r.x, y: r.y, w: r.width, h: r.height, via: "visibleRect"};
	}
	var text = a.x + "," + a.y + " " + a.w + "x" + a.h + " from " + a.via;
	if (text != areaLogged)
	{
		areaLogged = text;
		trace("NeedsWidget: screen area " + text);
	}
	return a;
}

// dx, dy: an extra offset in this movie's own units (an icon is 64 across at
// 100%), so a row or column can be spaced in icon sizes whatever the screen's
// shape. The dll works the offsets out; missing means none.
function setLayout(i, xPct, yPct, sizePct, alphaPct, showNumber, dx, dy)
{
	var c = icons[i].clip;
	var a = visibleArea();
	if (dx == undefined)
	{
		dx = 0;
	}
	if (dy == undefined)
	{
		dy = 0;
	}
	c._x = a.x + a.w * xPct / 100 + dx;
	c._y = a.y + a.h * yPct / 100 + dy;
	c._xscale = sizePct;
	c._yscale = sizePct;
	c._alpha = alphaPct;
	c.label._visible = showNumber;
	syncEdge(i);
}

// Where each icon on screen is, as shares of the screen: "id,x,y,halfW,halfH;" for
// each one showing, x and y its centre. The dll asks every frame while the
// settings' drag screen is open, and puts a handle there.
function spots()
{
	var out = "";
	if (!needs._visible)
	{
		return out;
	}
	var a = visibleArea();
	for (var i = 0; i < ICON_COUNT; i++)
	{
		var c = icons[i].clip;
		if (c._visible)
		{
			var half = ART_CANVAS / 2 * c._xscale / 100;
			out += i + "," + (c._x - a.x) / a.w + "," + (c._y - a.y) / a.h + "," + half / a.w + "," + half / a.h + ";";
		}
	}
	return out;
}

function setIcon(i, visible, fraction, stage, label)
{
	var o = icons[i];
	o.clip._visible = visible;
	if (fraction != o.fraction || stage != o.stage)
	{
		o.fraction = fraction;
		o.stage = stage;
		drawMeter(i);
		applyTint(i);
	}
	if (o.clip.label.text != label)
	{
		o.clip.label.text = label;
		syncEdge(i);
	}
}

function setShown(shown)
{
	needs._visible = shown;
}

// TEST10: fade the whole widget to alphaPct over ms (0 = at once). pulseTick runs it.
function setFade(alphaPct, ms)
{
	fade = {from: needs._alpha, to: alphaPct, start: getTimer(), ms: ms};
	if (ms <= 0)
	{
		needs._alpha = alphaPct;
	}
}

// TEST10: a need just got worse. Its glyph flashes bright and settles back over
// FLASH_MS. pulseTick runs it.
function flash(i)
{
	icons[i].flashAt = getTimer();
}

function flashTick(i, now)
{
	var o = icons[i];
	if (o.flashAt == undefined)
	{
		return;
	}
	var t = (now - o.flashAt) / FLASH_MS;
	if (t >= 1)
	{
		o.flashAt = undefined;
		applyTint(i);
		return;
	}
	var c = style.tintArt ? stageColor(o.stage) : 0xFFFFFF;
	var boost = FLASH_BRIGHT * (1 - t);
	new Color(o.clip.art).setTransform({ra: (c >> 16 & 255) / 2.55, ga: (c >> 8 & 255) / 2.55, ba: (c & 255) / 2.55, aa: 100, rb: boost, gb: boost, bb: boost, ab: 0});
}

// Every frame: an icon at its worst stage fades its glyph and meter down and back
// up once every PULSE_MS, so the need that most wants seeing catches the eye.
// Every other icon is held at full strength, which also undoes the fade the moment
// a need gets better or the pulse is switched off. The frame picture stays put.
function pulseTick()
{
	var now = getTimer();
	if (fade.ms > 0)
	{
		var f = Math.min(1, (now - fade.start) / fade.ms);
		needs._alpha = fade.from + (fade.to - fade.from) * f;
		if (f >= 1)
		{
			fade.ms = 0;
		}
	}
	var phase = (now % PULSE_MS) / PULSE_MS;
	var low = 100 - PULSE_DEPTH * (0.5 - 0.5 * Math.cos(phase * Math.PI * 2));
	for (var i = 0; i < ICON_COUNT; i++)
	{
		var o = icons[i];
		var a = style.pulse && o.stage >= 5 ? low : 100;
		o.clip.art._alpha = a;
		o.clip.meter._alpha = a;
		flashTick(i, now);
	}
}

// Scaleform keeps its extensions (Stage.visibleRect among them) off in each movie
// until that movie asks. Vanilla menus ask in their own first frame; this movie
// is loaded on its own, so it has to ask too, or every layout assumes 16:9.
_global.gfxExtensions = true;

var BASE_W = 1280;
var BASE_H = 720;
var areaLogged = "";
// The ten built-in needs, then 64 extra slots (Needs::kCount in the dll is 74 too).
var ICON_NAMES = ["hunger", "cold", "fatigue", "stress", "dirt", "encumbrance", "thirst", "wetness", "vitality", "disease"];
for (var extraSlot = 0; extraSlot < 64; extraSlot++)
{
	ICON_NAMES.push("");
}
var ICON_COUNT = ICON_NAMES.length;
var ART_PATHS = ["icons/", "NeedsWidget/icons/", "Interface/NeedsWidget/icons/"];
var ART_CANVAS = 64;
var PULSE_MS = 1600;
var PULSE_DEPTH = 55;

// Until NeedsWidget.dll sends the real style. frame stays undefined so the first
// setStyle loads the frames.
var style = {look: "ring", frame: undefined, artSize: 42, artY: 0, ringRadius: 27.5, ringWidth: 1.8, barLeft: -16.2, barTop: 21.8, barLength: 32.4, barHeight: 0.8, labelY: 31, tintArt: true, pulse: true};
var stageColors = [0xECE9E2, 0xE8DDB0, 0xE8C870, 0xE8A060, 0xD9503F, 0xFF2E1F];
var artDone = 0;
var frameDone = 0;

var LABEL_SIZE = 16;
var FLASH_MS = 700;
var FLASH_BRIGHT = 180;  // colour offset added at the start of a flash, out of 255
var fade = {from: 100, to: 100, start: 0, ms: 0};
var LABEL_X = -120;  // wide enough for a stage name like Severely Dehydrated
var LABEL_W = 240;
var LABEL_H = 24;
var numberStyle = {size: 100, tint: false, dx: 0, edge: true};
var EDGE_X = [-1, 1, -1, 1];
var EDGE_Y = [-1, -1, 1, 1];
var labelFormat = new TextFormat();
labelFormat.font = "$EverywhereMediumFont";
labelFormat.size = LABEL_SIZE;
labelFormat.color = 0xFFFFFF;
labelFormat.align = "center";

var needs = this.createEmptyMovieClip("needs", 10);
var icons = [];
for (var n = 0; n < ICON_COUNT; n++)
{
	var clip = needs.createEmptyMovieClip("icon" + n, n + 1);
	clip.createEmptyMovieClip("meter", 2);
	for (var e = 0; e < 4; e++)
	{
		clip.createTextField("edge" + e, 4 + e, LABEL_X + EDGE_X[e], 31 + EDGE_Y[e], LABEL_W, LABEL_H);
		var t = clip["edge" + e];
		t.embedFonts = true;
		t.selectable = false;
		t.setNewTextFormat(labelFormat);
		t.textColor = 0x000000;
		t.text = "";
		t._visible = false;
	}
	clip.createTextField("label", 8, LABEL_X, 31, LABEL_W, LABEL_H);
	clip.label.embedFonts = true;
	clip.label.selectable = false;
	clip.label.setNewTextFormat(labelFormat);
	clip.label.text = "";
	clip.label._visible = false;
	clip._visible = false;
	icons.push({clip: clip, fraction: 0, stage: 0, source: undefined, frameSource: undefined});
	loadPicture(n, "art", 0);
}
needs._visible = false;
needs.onEnterFrame = pulseTick;
