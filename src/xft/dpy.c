#include "xftint.h"

XftDisplayInfo info;

static int _XftCloseDisplay(Display* dpy, XExtCodes* codes) {
	// Get rid of any dangling unreferenced fonts
	info.max_unref_fonts = 0;
	XftFontManageMemory();
	
	// Clean up the default values
	if (info.defaults)
		FcPatternDestroy(info.defaults);
	
	return 0;
}

static FcPattern* _XftDefaultInit();

static FcResult _XftDefaultGet(const char* object, FcValue* v) {
	if (!info.defaults)
		return FcResultNoMatch;
	FcResult	r = FcPatternGet(info.defaults, object, W.scr, v);
	if (r == FcResultNoId && W.scr > 0)
		r = FcPatternGet(info.defaults, object, 0, v);
	return r;
}

static bool XftDefaultGetBool(const char* object, bool def) {
	FcValue v;
	FcResult r = _XftDefaultGet(object, &v);
	if (r != FcResultMatch || v.type != FcTypeBool)
		return def;
	return v.u.b;
}

static int XftDefaultGetInteger(const char* object, int def) {
	FcValue v;
	FcResult r = _XftDefaultGet(object, &v);
	if (r != FcResultMatch || v.type != FcTypeInteger)
		return def;
	return v.u.i;
}

static double XftDefaultGetDouble(const char* object, double def) {
	FcValue v;
	FcResult r = _XftDefaultGet(object, &v);
	if (r != FcResultMatch || v.type != FcTypeDouble)
		return def;
	return v.u.d;
}

void XftDisplayInfoInit(void) {
	info.codes = XAddExtension(W.d); // what does this do?
	if (!info.codes)
		goto bail1;
	XESetCloseDisplay(W.d, info.codes->extension, _XftCloseDisplay);
	
	for (int i=0; i<XFT_NUM_SOLID_COLOR; i++) {
		info.colors[i].screen = -1;
		info.colors[i].pict = 0;
	}
	info.fonts = NULL;
	
	info.glyph_memory = 0;
	info.max_glyph_memory = XftDefaultGetInteger(XFT_MAX_GLYPH_MEMORY, XFT_DPY_MAX_GLYPH_MEMORY);
	if (XftDebug() & XFT_DBG_CACHE)
		printf ("global max cache memory %ld\n", info.max_glyph_memory);
	
	info.num_unref_fonts = 0;
	info.max_unref_fonts = XftDefaultGetInteger(XFT_MAX_UNREF_FONTS, XFT_DPY_MAX_UNREF_FONTS);
	if (XftDebug() & XFT_DBG_CACHE)
		printf ("global max unref fonts %d\n", info.max_unref_fonts);
	
	memset(info.fontHash, '\0', sizeof(XftFont*)*XFT_NUM_FONT_HASH);
	
	info.defaults = _XftDefaultInit();
	
 bail1:
	if (XftDebug() & XFT_DBG_RENDER) {
		printf ("XftDisplayInfoGet failed to initialize, Xft unhappy :(\n");
	}
}

// Reduce memory usage in X server

static void _XftDisplayValidateMemory(void) {
	unsigned long glyph_memory = 0;
	XftFontInt* font;
	for (XftFont* public=info.fonts; public; public=font->next) {
		font = (XftFontInt*)public;
		glyph_memory += font->glyph_memory;
	}
	if (glyph_memory != info.glyph_memory)
		printf("Display glyph cache incorrect has %ld bytes, should have %ld\n", info.glyph_memory, glyph_memory);
}

_X_HIDDEN void _XftDisplayManageMemory(void) {
	if (!info.max_glyph_memory)
		return;
	
	if (XftDebug () & XFT_DBG_CACHE) {
		if (info.glyph_memory > info.max_glyph_memory)
			printf ("Reduce global memory from %ld to %ld\n",
			        info.glyph_memory, info.max_glyph_memory);
		_XftDisplayValidateMemory();
	}
	
	while (info.glyph_memory > info.max_glyph_memory) {
		unsigned long glyph_memory = rand () % info.glyph_memory;
		XftFont* public = info.fonts;
		while (public) {
			XftFontInt* font = (XftFontInt*)public;
			
			if (font->glyph_memory > glyph_memory) {
				_XftFontUncacheGlyph(public);
				break;
			}
			public = font->next;
			glyph_memory -= font->glyph_memory;
		}
	}
	if (XftDebug() & XFT_DBG_CACHE)
		_XftDisplayValidateMemory();
}

bool XftDefaultSet(FcPattern* defaults) {
	if (info.defaults)
		FcPatternDestroy(info.defaults);
	info.defaults = defaults;
	if (!info.max_glyph_memory)
		info.max_glyph_memory = XFT_DPY_MAX_GLYPH_MEMORY;
	info.max_glyph_memory = XftDefaultGetInteger(XFT_MAX_GLYPH_MEMORY, info.max_glyph_memory);
	if (!info.max_unref_fonts)
		info.max_unref_fonts = XFT_DPY_MAX_UNREF_FONTS;
	info.max_unref_fonts = XftDefaultGetInteger(XFT_MAX_UNREF_FONTS, info.max_unref_fonts);
	return true;
}

_X_HIDDEN int XftDefaultParseBool(const char* v) {
	char c0 = *v;
	if (isupper(c0))
		c0 = tolower(c0);
	if (c0 == 't' || c0 == 'y' || c0 == '1')
		return 1;
	if (c0 == 'f' || c0 == 'n' || c0 == '0')
		return 0;
	if (c0 == 'o') {
		char c1 = v[1];
		if (isupper(c1))
			c1 = tolower(c1);
		if (c1 == 'n')
			return 1;
		if (c1 == 'f')
			return 0;
	}
	return -1;
}

static bool _XftDefaultInitBool(FcPattern* pat, const char* option) {
	int i;
	char* v = XGetDefault(W.d, "Xft", option);
	if (v && (i = XftDefaultParseBool(v)) >= 0)
		return FcPatternAddBool(pat, option, i!=0);
	return true;
}

static bool _XftDefaultInitDouble(FcPattern* pat, const char* option) {
	char* v = XGetDefault(W.d, "Xft", option);
	if (v) {
		char* e;
		double d = strtod(v, &e);
		if (e != v)
			return FcPatternAddDouble(pat, option, d);
	}
	return true;
}

static bool _XftDefaultInitInteger(FcPattern *pat, const char *option) {
	char* v = XGetDefault(W.d, "Xft", option);
	if (v) {
		int i;
		if (FcNameConstant((FcChar8*)v, &i))
			return FcPatternAddInteger(pat, option, i);
		char* e;
		i = strtol(v, &e, 0);
		if (e != v)
			return FcPatternAddInteger(pat, option, i);
	}
	return true;
}

static FcPattern* _XftDefaultInit(void) {
	FcPattern* pat = FcPatternCreate();
	if (!pat)
		goto bail0;
	
	if (!_XftDefaultInitDouble(pat, FC_SCALE))
		goto bail1;
	if (!_XftDefaultInitDouble(pat, FC_DPI))
		goto bail1;
	if (!_XftDefaultInitInteger(pat, FC_RGBA))
		goto bail1;
	if (!_XftDefaultInitInteger(pat, FC_LCD_FILTER))
		goto bail1;
	if (!_XftDefaultInitBool(pat, FC_ANTIALIAS))
		goto bail1;
	if (!_XftDefaultInitBool(pat, FC_EMBOLDEN))
		goto bail1;
	if (!_XftDefaultInitBool(pat, FC_AUTOHINT))
		goto bail1;
	if (!_XftDefaultInitInteger(pat, FC_HINT_STYLE))
		goto bail1;
	if (!_XftDefaultInitBool(pat, FC_HINTING))
		goto bail1;
	if (!_XftDefaultInitBool(pat, FC_MINSPACE))
		goto bail1;
	if (!_XftDefaultInitInteger(pat, XFT_MAX_GLYPH_MEMORY))
		goto bail1;
	
	return pat;

 bail1:
	FcPatternDestroy(pat);
 bail0:
	return NULL;
}

void XftDefaultSubstitute(FcPattern* pattern) {
	FcValue v;
	if (FcPatternGet(pattern, FC_ANTIALIAS, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_ANTIALIAS, XftDefaultGetBool(FC_ANTIALIAS, true));
	if (FcPatternGet(pattern, FC_EMBOLDEN, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_EMBOLDEN, XftDefaultGetBool(FC_EMBOLDEN, false));
	if (FcPatternGet(pattern, FC_HINTING, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_HINTING, XftDefaultGetBool(FC_HINTING, true));
	if (FcPatternGet(pattern, FC_HINT_STYLE, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, FC_HINT_STYLE, XftDefaultGetInteger(FC_HINT_STYLE, FC_HINT_FULL));
	if (FcPatternGet(pattern, FC_AUTOHINT, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_AUTOHINT, XftDefaultGetBool(FC_AUTOHINT, false));
	// subpixel order
	if (FcPatternGet(pattern, FC_RGBA, 0, &v) == FcResultNoMatch) {
		int subpixel = FC_RGBA_UNKNOWN;
		int render_order = XRenderQuerySubpixelOrder(W.d, W.scr);
		switch (render_order) {
		default:
		case SubPixelUnknown: subpixel = FC_RGBA_UNKNOWN; break;
		case SubPixelHorizontalRGB: subpixel = FC_RGBA_RGB; break;
		case SubPixelHorizontalBGR: subpixel = FC_RGBA_BGR; break;
		case SubPixelVerticalRGB: subpixel = FC_RGBA_VRGB; break;
		case SubPixelVerticalBGR: subpixel = FC_RGBA_VBGR; break;
		case SubPixelNone: subpixel = FC_RGBA_NONE; break;
		}
		FcPatternAddInteger(pattern, FC_RGBA, XftDefaultGetInteger(FC_RGBA, subpixel));
	}
	if (FcPatternGet(pattern, FC_LCD_FILTER, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, FC_LCD_FILTER, XftDefaultGetInteger(FC_LCD_FILTER, FC_LCD_DEFAULT));
	if (FcPatternGet(pattern, FC_MINSPACE, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_MINSPACE, XftDefaultGetBool(FC_MINSPACE, false));
	
	if (FcPatternGet(pattern, FC_DPI, 0, &v) == FcResultNoMatch) {
		double dpi = (double)DisplayHeight(W.d, W.scr)*25.4 / DisplayHeightMM(W.d, W.scr);
		FcPatternAddDouble(pattern, FC_DPI, XftDefaultGetDouble(FC_DPI, dpi));
	}
	if (FcPatternGet(pattern, FC_SCALE, 0, &v) == FcResultNoMatch)
		FcPatternAddDouble(pattern, FC_SCALE, XftDefaultGetDouble(FC_SCALE, 1.0));
	
	if (FcPatternGet(pattern, XFT_MAX_GLYPH_MEMORY, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, XFT_MAX_GLYPH_MEMORY, XftDefaultGetInteger(XFT_MAX_GLYPH_MEMORY, XFT_FONT_MAX_GLYPH_MEMORY));
	FcDefaultSubstitute(pattern);
}

