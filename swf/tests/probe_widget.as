// Ruffle probe for widget.swf (TEST10). Loads it the way Skyrim will (as a whole
// movie), lets it load its pictures, drives the same functions NeedsWidget.dll
// calls, and traces what happened. build_widget.py --probe lays the pictures out
// (eight glyphs in icons/, dirt only in NeedsWidget/icons/, encumbrance missing)
// and compares these lines against the values it expects.
// Ruffle does not validate Scaleform-only behaviour (visibleRect, $Everywhere
// fonts, how Skyrim resolves paths), so this catches AS2 logic, loading and
// drawing mistakes, not engine ones.
//
// Functions first: FFDec's compiler does not hoist them.

function r2(v)
{
	return Math.round(v * 100) / 100;
}

function bounds(mc)
{
	var b = mc.getBounds(mc);
	return r2(b.xMin) + "," + r2(b.yMin) + "," + r2(b.xMax) + "," + r2(b.yMax);
}

function tintOf(mc)
{
	var t = new Color(mc).getTransform();
	return Math.round(t.ra) + "," + Math.round(t.ga) + "," + Math.round(t.ba);
}

// Calls then() once test() is true, or after 150 frames whatever happens, so a
// load that never finishes shows up as a wrong line rather than a hang.
function waitFor(test, then)
{
	var frames = 0;
	poll.onEnterFrame = function()
	{
		frames++;
		if (test() || frames > 150)
		{
			delete this.onEnterFrame;
			then();
		}
	};
}

// Runs perFrame() on each of the next n frames, then then(). (Not "each": FFDec reads that as for each.)
function everyFrame(n, perFrame, then)
{
	var frames = 0;
	poll.onEnterFrame = function()
	{
		frames++;
		perFrame();
		if (frames >= n)
		{
			delete this.onEnterFrame;
			then();
		}
	};
}

function styleArgs(look, frame, artSize, artY, labelY, tint, pulse)
{
	w.setStyle(look, frame, artSize, artY, 27.5, 1.8, -16.2, 21.8, 32.4, 0.8, labelY, tint, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFF6633, 0xFF9966, 0xCC3300, pulse);
}

function stepArt()
{
	for (var i = 0; i < w.icons.length; i++)
	{
		var o = w.icons[i];
		// An empty extra slot loads nothing; its empty clip's bounds mean nothing.
		trace("NWPROBE art " + i + " " + o.source + (o.source == "none" ? "" : " bounds=" + bounds(o.clip.art)));
	}
	styleArgs("ring", "frame_ring", 36, 0, 31, true, true);
	waitFor(function()
	{
		return w.frameDone >= w.icons.length;
	}, stepRing);
}

function stepRing()
{
	var o = w.icons[0];
	var c = o.clip;
	trace("NWPROBE frame ring " + w.frameDone + " " + o.frameSource + " bounds=" + bounds(c.frame) + " x=" + c.frame._x + " y=" + c.frame._y);
	w.setShown(true);
	w.setLayout(0, 50, 50, 150, 80, true);
	trace("NWPROBE layout x=" + c._x + " y=" + c._y + " xs=" + c._xscale + " a=" + Math.round(c._alpha) + " label=" + c.label._visible + " labelY=" + c.label._y);
	trace("NWPROBE place xs=" + c.art._xscale + " x=" + c.art._x + " y=" + c.art._y);
	w.setIcon(0, true, 0.5, 3, "50%");
	trace("NWPROBE half visible=" + c._visible + " text=" + c.label.text + " meter=" + bounds(c.meter) + " tint=" + tintOf(c.art));
	w.setIcon(0, true, 0.25, 5, "25%");
	trace("NWPROBE quarter meter=" + bounds(c.meter) + " tint=" + tintOf(c.art));
	// Stage 5 with the pulse on: over 2 seconds (more than one 1.6 s cycle) the
	// glyph must dip well down and come back up, with the meter in step and the
	// frame picture left alone.
	var low = 100;
	var high = 0;
	var inStep = true;
	var frameStill = true;
	everyFrame(60, function()
	{
		var a = c.art._alpha;
		low = Math.min(low, a);
		high = Math.max(high, a);
		inStep = inStep && Math.abs(c.meter._alpha - a) < 1;
		frameStill = frameStill && c.frame._alpha == 100;
	}, function()
	{
		trace("NWPROBE pulse dips=" + (low < 60) + " peaks=" + (high > 95) + " meterInStep=" + inStep + " frameStill=" + frameStill);
		stepEmpty();
	});
}

function stepEmpty()
{
	var c = w.icons[0].clip;
	w.setIcon(0, true, 0, 0, "0%");
	// Once the need gets better, the fade must be gone by the next frame.
	everyFrame(2, function()
	{
	}, function()
	{
		trace("NWPROBE empty meterW=" + c.meter._width + " tint=" + tintOf(c.art) + " a=" + Math.round(c.art._alpha));
		styleArgs("bar", "frame_bar", 38, -4, 25, true, false);
		waitFor(function()
		{
			return w.frameDone >= 2 * w.icons.length;
		}, stepBar);
	});
}

function stepBar()
{
	var o = w.icons[0];
	var c = o.clip;
	trace("NWPROBE frame bar " + w.frameDone + " " + o.frameSource + " bounds=" + bounds(c.frame));
	w.setIcon(0, true, 0.5, 3, "50%");
	trace("NWPROBE bar meter=" + bounds(c.meter) + " place xs=" + c.art._xscale + " x=" + c.art._x + " y=" + c.art._y + " labelY=" + c.label._y);
	// Stage 5 with the pulse switched off: the glyph must stay at full strength.
	w.setIcon(0, true, 0.5, 5, "50%");
	var low = 100;
	everyFrame(60, function()
	{
		low = Math.min(low, c.art._alpha);
	}, function()
	{
		trace("NWPROBE nopulse min=" + Math.round(low));
		stepIcon();
	});
}

function stepIcon()
{
	var o = w.icons[0];
	var c = o.clip;
	styleArgs("icon", "", 36, 0, 31, false, false);
	w.setIcon(0, true, 0.75, 4, "75%");
	trace("NWPROBE icon frames=" + w.frameDone + " frame=" + o.frameSource + " frameW=" + c.frame._width + " meterW=" + c.meter._width + " tint=" + tintOf(c.art));
	// Numbers twice the size, coloured like the icon (stage 4 here), nudged right;
	// then back to plain white at the normal size.
	w.setNumbers(200, true, 10);
	trace("NWPROBE numbers x=" + c.label._x + " w=" + c.label._width + " size=" + c.label.getTextFormat().size + " color=" + c.label.textColor.toString(16));
	// The dark edge (2.3.0): black copies behind, nudged by the text's scale; off hides them.
	trace("NWPROBE edge x=" + c.edge0._x + " dy=" + (c.edge0._y - c.label._y) + " text=" + c.edge0.text + " color=" + c.edge0.textColor.toString(16) + " on=" + c.edge0._visible);
	w.setNumbers(200, true, 10, false);
	trace("NWPROBE edgeoff on=" + c.edge0._visible);
	w.setNumbers(100, false, 0);
	trace("NWPROBE plain x=" + c.label._x + " w=" + c.label._width + " size=" + c.label.getTextFormat().size + " color=" + c.label.textColor.toString(16));
	w.setLayout(1, 0, 100, 100, 100, false);
	var c1 = w.icons[1].clip;
	trace("NWPROBE corner x=" + c1._x + " y=" + c1._y);
	// A row or column step: the dll's offset is added after the percent position.
	w.setLayout(1, 50, 50, 100, 100, false, -51.5, 25);
	trace("NWPROBE offset x=" + c1._x + " y=" + c1._y);
	w.setIcon(0, false, 0.5, 3, "50%");
	w.setShown(false);
	trace("NWPROBE hidden icon=" + c._visible + " shown=" + w.needs._visible);
	stepFx();
}

// TEST10: fading the whole widget, and the flash on an icon whose need got worse.
function stepFx()
{
	var c = w.icons[0].clip;
	w.setFade(0, 0);
	trace("NWPROBE fade out a=" + Math.round(w.needs._alpha));
	w.setFade(100, 100);
	everyFrame(20, function()
	{
	}, function()
	{
		trace("NWPROBE fade in a=" + Math.round(w.needs._alpha));
		w.flash(0);
		var peak = 0;
		everyFrame(3, function()
		{
			peak = Math.max(peak, new Color(c.art).getTransform().rb);
		}, function()
		{
			trace("NWPROBE flash bright=" + (peak > 50));
			everyFrame(40, function()
			{
			}, function()
			{
				trace("NWPROBE flash over rb=" + Math.round(new Color(c.art).getTransform().rb));
				stepExtra();
			});
		});
	});
}

// Extra icons: the dll names an addon's picture for an empty slot with setArt.
// The same name a second time must not load it again.
function stepExtra()
{
	var before = w.artDone;
	w.setArt(10, "cold");
	waitFor(function()
	{
		return w.artDone > before;
	}, function()
	{
		var o = w.icons[10];
		trace("NWPROBE extra art 10 " + o.source + " bounds=" + bounds(o.clip.art));
		w.setArt(10, "cold");
		// Loads finish frames later, so give a second one time to show up.
		everyFrame(30, function()
		{
		}, function()
		{
			trace("NWPROBE extra again loads=" + (w.artDone - before));
			trace("NWPROBE done");
		});
	});
}

var w;
var host = this.createEmptyMovieClip("host", 1);
var poll = this.createEmptyMovieClip("poll", 2);
var mcl = new MovieClipLoader();
var listener = {};
listener.onLoadInit = function(target)
{
	w = target;
	trace("NWPROBE start shown=" + w.needs._visible + " icons=" + w.icons.length + " ext=" + _global.gfxExtensions);
	waitFor(function()
	{
		return w.artDone >= w.icons.length;
	}, stepArt);
};
mcl.addListener(listener);
mcl.loadClip("widget.swf", host);
