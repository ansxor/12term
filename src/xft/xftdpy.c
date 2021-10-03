#include "xftint.h"

_X_HIDDEN XftDisplayInfo* _XftDisplayInfo;

static int _XftCloseDisplay(Display* dpy, XExtCodes* codes) {
	XftDisplayInfo* info = _XftDisplayInfoGet(dpy, false);
	if (!info)
		return 0;
	
	// Get rid of any dangling unreferenced fonts
	info->max_unref_fonts = 0;
	XftFontManageMemory(dpy);
	
	// Clean up the default values
	if (info->defaults)
		FcPatternDestroy(info->defaults);
	
	// Unhook from the global list
	XftDisplayInfo** prev;
	for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next)
		if (info->display==dpy)
			break;
	*prev = info->next;
	
	free(info);
	return 0;
}


_X_HIDDEN XftDisplayInfo* _XftDisplayInfoGet(Display* dpy, bool createIfNecessary) {
	XftDisplayInfo* info;
	XftDisplayInfo** prev;
	for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next) {
		if (info->display == dpy) {
			// MRU the list
			if (prev != &_XftDisplayInfo) {
				*prev = info->next;
				info->next = _XftDisplayInfo;
				_XftDisplayInfo = info;
			}
			return info;
		}
	}
	if (!createIfNecessary)
		return NULL;

	info = malloc(sizeof(XftDisplayInfo));
	if (!info)
		goto bail0;
	info->codes = XAddExtension(dpy);
	if (!info->codes)
		goto bail1;
	XESetCloseDisplay(dpy, info->codes->extension, _XftCloseDisplay);

	info->display = dpy;
	info->defaults = NULL;
	info->solidFormat = NULL;
	info->use_free_glyphs = true;
	
	int major, minor;
	XRenderQueryVersion(dpy, &major, &minor);
	if (major<0 || (major==0 && minor<=2))
		info->use_free_glyphs = false;
		
	info->hasSolid = false;
	if (major>0 || (major==0 && minor>=10))
		info->hasSolid = true;
	
	info->solidFormat = XRenderFindFormat(
		dpy,
		PictFormatType|PictFormatDepth|PictFormatRedMask|PictFormatGreenMask|PictFormatBlueMask|PictFormatAlphaMask,
		&(XRenderPictFormat){
			.type = PictTypeDirect,
			.depth = 32,
			.direct.redMask = 0xFF,
			.direct.greenMask = 0xFF,
			.direct.blueMask = 0xFF,
			.direct.alphaMask = 0xFF,
		}, 0);
	if (XftDebug() & XFT_DBG_RENDER) {
		Visual* visual = DefaultVisual(dpy, DefaultScreen(dpy));
		XRenderPictFormat* format = XRenderFindVisualFormat(dpy, visual);
		
		printf ("XftDisplayInfoGet Default visual 0x%x ",
		        (int)visual->visualid);
		if (format) {
			if (format->type == PictTypeDirect) {
				printf("format %d,%d,%d,%d\n",
				        format->direct.alpha,
				        format->direct.red,
				        format->direct.green,
				        format->direct.blue);
			} else {
				printf("format indexed\n");
			}
		} else
			printf("No Render format for default visual\n");
		printf("XftDisplayInfoGet initialized");
	}
	for (int i=0; i<XFT_NUM_SOLID_COLOR; i++) {
		info->colors[i].screen = -1;
		info->colors[i].pict = 0;
	}
	info->fonts = NULL;
	
	info->next = _XftDisplayInfo;
	_XftDisplayInfo = info;
	
	info->glyph_memory = 0;
	info->max_glyph_memory = XftDefaultGetInteger(dpy, XFT_MAX_GLYPH_MEMORY, 0, XFT_DPY_MAX_GLYPH_MEMORY);
	if (XftDebug() & XFT_DBG_CACHE)
		printf ("global max cache memory %ld\n", info->max_glyph_memory);
	
	info->num_unref_fonts = 0;
	info->max_unref_fonts = XftDefaultGetInteger(dpy, XFT_MAX_UNREF_FONTS, 0, XFT_DPY_MAX_UNREF_FONTS);
	if (XftDebug() & XFT_DBG_CACHE)
		printf ("global max unref fonts %d\n", info->max_unref_fonts);
	
	memset(info->fontHash, '\0', sizeof(XftFont*)*XFT_NUM_FONT_HASH);
	return info;
	
 bail1:
	free (info);
 bail0:
	if (XftDebug() & XFT_DBG_RENDER) {
		printf ("XftDisplayInfoGet failed to initialize, Xft unhappy :(\n");
	}
	return NULL;
}

// Reduce memory usage in X server

static void _XftDisplayValidateMemory(XftDisplayInfo* info) {
	unsigned long glyph_memory = 0;
	XftFontInt* font;
	for (XftFont* public=info->fonts; public; public=font->next) {
		font = (XftFontInt*)public;
		glyph_memory += font->glyph_memory;
	}
	if (glyph_memory != info->glyph_memory)
		printf("Display glyph cache incorrect has %ld bytes, should have %ld\n", info->glyph_memory, glyph_memory);
}

_X_HIDDEN void _XftDisplayManageMemory(Display* dpy) {
	XftDisplayInfo* info = _XftDisplayInfoGet(dpy, false);
	if (!info || !info->max_glyph_memory)
		return;
	
	if (XftDebug () & XFT_DBG_CACHE) {
		if (info->glyph_memory > info->max_glyph_memory)
			printf ("Reduce global memory from %ld to %ld\n",
			        info->glyph_memory, info->max_glyph_memory);
		_XftDisplayValidateMemory(info);
	}
	
	while (info->glyph_memory > info->max_glyph_memory) {
		unsigned long glyph_memory = rand () % info->glyph_memory;
		XftFont* public = info->fonts;
		while (public) {
			XftFontInt* font = (XftFontInt*)public;
			
			if (font->glyph_memory > glyph_memory) {
				_XftFontUncacheGlyph(dpy, public);
				break;
			}
			public = font->next;
			glyph_memory -= font->glyph_memory;
		}
	}
	if (XftDebug() & XFT_DBG_CACHE)
		_XftDisplayValidateMemory (info);
}

_X_EXPORT Bool XftDefaultSet(Display* dpy, FcPattern* defaults) {
	XftDisplayInfo* info = _XftDisplayInfoGet(dpy, true);
	
	if (!info)
		return false;
	if (info->defaults)
		FcPatternDestroy (info->defaults);
	info->defaults = defaults;
	if (!info->max_glyph_memory)
		info->max_glyph_memory = XFT_DPY_MAX_GLYPH_MEMORY;
	info->max_glyph_memory = XftDefaultGetInteger(dpy, XFT_MAX_GLYPH_MEMORY, 0, info->max_glyph_memory);
	if (!info->max_unref_fonts)
		info->max_unref_fonts = XFT_DPY_MAX_UNREF_FONTS;
	info->max_unref_fonts = XftDefaultGetInteger(dpy, XFT_MAX_UNREF_FONTS, 0, info->max_unref_fonts);
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

static Bool _XftDefaultInitBool(Display* dpy, FcPattern* pat, const char* option) {
	int i;
	char* v = XGetDefault(dpy, "Xft", option);
	if (v && (i = XftDefaultParseBool (v)) >= 0)
		return FcPatternAddBool(pat, option, i != 0);
	return true;
}

static Bool _XftDefaultInitDouble(Display* dpy, FcPattern* pat, const char* option) {
	char* v = XGetDefault(dpy, "Xft", option);
	if (v) {
		char* e;
		double d = strtod(v, &e);
		if (e != v)
			return FcPatternAddDouble(pat, option, d);
	}
	return true;
}

static Bool
_XftDefaultInitInteger(Display *dpy, FcPattern *pat, const char *option) {
	char* v = XGetDefault(dpy, "Xft", option);
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

static FcPattern* _XftDefaultInit(Display* dpy) {
	FcPattern* pat = FcPatternCreate();
	if (!pat)
		goto bail0;
	
	if (!_XftDefaultInitDouble(dpy, pat, FC_SCALE))
		goto bail1;
	if (!_XftDefaultInitDouble(dpy, pat, FC_DPI))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, XFT_RENDER))
		goto bail1;
	if (!_XftDefaultInitInteger(dpy, pat, FC_RGBA))
		goto bail1;
	if (!_XftDefaultInitInteger(dpy, pat, FC_LCD_FILTER))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, FC_ANTIALIAS))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, FC_EMBOLDEN))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, FC_AUTOHINT))
		goto bail1;
	if (!_XftDefaultInitInteger(dpy, pat, FC_HINT_STYLE))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, FC_HINTING))
		goto bail1;
	if (!_XftDefaultInitBool(dpy, pat, FC_MINSPACE))
		goto bail1;
	if (!_XftDefaultInitInteger(dpy, pat, XFT_MAX_GLYPH_MEMORY))
		goto bail1;
	
	return pat;

 bail1:
	FcPatternDestroy(pat);
 bail0:
	return NULL;
}

static FcResult _XftDefaultGet(Display* dpy, const char* object, int screen, FcValue* v) {
	XftDisplayInfo* info = _XftDisplayInfoGet(dpy, true);
	if (!info)
		return FcResultNoMatch;
	
	if (!info->defaults) {
		info->defaults = _XftDefaultInit(dpy);
		if (!info->defaults)
			return FcResultNoMatch;
	}
	FcResult	r = FcPatternGet(info->defaults, object, screen, v);
	if (r == FcResultNoId && screen > 0)
		r = FcPatternGet (info->defaults, object, 0, v);
	return r;
}

_X_HIDDEN bool XftDefaultGetBool(Display* dpy, const char* object, int screen, bool def) {
	FcValue v;
	FcResult r = _XftDefaultGet(dpy, object, screen, &v);
	if (r != FcResultMatch || v.type != FcTypeBool)
		return def;
	return v.u.b;
}

_X_HIDDEN int XftDefaultGetInteger(Display* dpy, const char* object, int screen, int def) {
	FcValue v;
	FcResult r = _XftDefaultGet (dpy, object, screen, &v);
	if (r != FcResultMatch || v.type != FcTypeInteger)
		return def;
	return v.u.i;
}

_X_HIDDEN double XftDefaultGetDouble(Display* dpy, const char* object, int screen, double def) {
	FcValue v;
	FcResult r = _XftDefaultGet(dpy, object, screen, &v);
	if (r != FcResultMatch || v.type != FcTypeDouble)
		return def;
	return v.u.d;
}

void XftDefaultSubstitute(Display* dpy, int screen, FcPattern* pattern) {
	FcValue v;
	if (FcPatternGet(pattern, XFT_RENDER, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, XFT_RENDER, XftDefaultGetBool(dpy, XFT_RENDER, screen, true));
	if (FcPatternGet(pattern, FC_ANTIALIAS, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_ANTIALIAS, XftDefaultGetBool(dpy, FC_ANTIALIAS, screen, true));
	if (FcPatternGet(pattern, FC_EMBOLDEN, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_EMBOLDEN, XftDefaultGetBool(dpy, FC_EMBOLDEN, screen, false));
	if (FcPatternGet(pattern, FC_HINTING, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_HINTING, XftDefaultGetBool(dpy, FC_HINTING, screen, true));
	if (FcPatternGet(pattern, FC_HINT_STYLE, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, FC_HINT_STYLE, XftDefaultGetInteger(dpy, FC_HINT_STYLE, screen, FC_HINT_FULL));
	if (FcPatternGet(pattern, FC_AUTOHINT, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_AUTOHINT, XftDefaultGetBool(dpy, FC_AUTOHINT, screen, false));
	// subpixel order
	if (FcPatternGet(pattern, FC_RGBA, 0, &v) == FcResultNoMatch) {
		int subpixel = FC_RGBA_UNKNOWN;
		int render_order = XRenderQuerySubpixelOrder(dpy, screen);
		switch (render_order) {
		default:
		case SubPixelUnknown: subpixel = FC_RGBA_UNKNOWN; break;
		case SubPixelHorizontalRGB: subpixel = FC_RGBA_RGB; break;
		case SubPixelHorizontalBGR: subpixel = FC_RGBA_BGR; break;
		case SubPixelVerticalRGB: subpixel = FC_RGBA_VRGB; break;
		case SubPixelVerticalBGR: subpixel = FC_RGBA_VBGR; break;
		case SubPixelNone: subpixel = FC_RGBA_NONE; break;
		}
		FcPatternAddInteger(pattern, FC_RGBA, XftDefaultGetInteger(dpy, FC_RGBA, screen, subpixel));
	}
	if (FcPatternGet(pattern, FC_LCD_FILTER, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, FC_LCD_FILTER, XftDefaultGetInteger(dpy, FC_LCD_FILTER, screen, FC_LCD_DEFAULT));
	if (FcPatternGet(pattern, FC_MINSPACE, 0, &v) == FcResultNoMatch)
		FcPatternAddBool(pattern, FC_MINSPACE, XftDefaultGetBool (dpy, FC_MINSPACE, screen, false));
	
	if (FcPatternGet(pattern, FC_DPI, 0, &v) == FcResultNoMatch) {
		double dpi = (double)DisplayHeight(dpy, screen)*25.4 / DisplayHeightMM(dpy, screen);
		FcPatternAddDouble(pattern, FC_DPI, XftDefaultGetDouble(dpy, FC_DPI, screen, dpi));
	}
	if (FcPatternGet(pattern, FC_SCALE, 0, &v) == FcResultNoMatch)
		FcPatternAddDouble(pattern, FC_SCALE, XftDefaultGetDouble(dpy, FC_SCALE, screen, 1.0));
	
	if (FcPatternGet(pattern, XFT_MAX_GLYPH_MEMORY, 0, &v) == FcResultNoMatch)
		FcPatternAddInteger(pattern, XFT_MAX_GLYPH_MEMORY, XftDefaultGetInteger(dpy, XFT_MAX_GLYPH_MEMORY, screen, XFT_FONT_MAX_GLYPH_MEMORY));
	FcDefaultSubstitute(pattern);
}

