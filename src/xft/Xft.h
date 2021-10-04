#pragma once

#include <stdarg.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <X11/extensions/Xrender.h>

#define XFT_MAX_GLYPH_MEMORY	"maxglyphmemory"
#define XFT_MAX_UNREF_FONTS	"maxunreffonts"

extern FT_Library	_XftFTlibrary;

typedef struct XftFontInfo XftFontInfo;

typedef struct XftFont {
	int ascent, descent, height;
	int max_advance_width;
	FcCharSet* charset;
	FcPattern* pattern;
} XftFont;

typedef struct XftDraw XftDraw;

typedef struct XftGlyphFontSpec {
	XftFont* font;
	FT_UInt glyph;
	short	x, y;
} XftGlyphFontSpec;

// ghhh
typedef struct XftGlyphFont {
	XftFont* font;
	FT_UInt glyph;
} XftGlyphFont;

/* xftcolor.c */
unsigned long XftColorAllocValue(const XRenderColor* color);

/* xftdpy.c */
bool XftDefaultSet(FcPattern* defaults);
void XftDefaultSubstitute(FcPattern* pattern);
void XftDisplayInfoInit(void);

/* xftdraw.c */
XftDraw* XftDrawCreate(Drawable drawable);
void XftDrawChange(XftDraw* draw, Drawable drawable);

Drawable XftDrawDrawable(XftDraw* draw);

void XftDrawDestroy(XftDraw* draw);

Picture XftDrawPicture(XftDraw* draw);

Picture XftDrawSrcPicture(const XRenderColor* color);

void XftDrawRect(XftDraw* draw, const XRenderColor* color, int x, int y, unsigned int width, unsigned int height);

bool XftDrawSetClip(XftDraw* draw, Region r);

bool XftDrawSetClipRectangles(XftDraw* draw, int xOrigin, int yOrigin, const XRectangle* rects, int n);

void XftDrawSetSubwindowMode(XftDraw* draw, int mode);

/* xftextent.c */

void XftGlyphExtents(XftFont* pub, const FT_UInt* glyphs, int nglyphs, XGlyphInfo* extents);
void XftTextExtents32(XftFont* pub, const FcChar32* string, int len, XGlyphInfo* extents);

/* xftfreetype.c */

FT_Face XftLockFace(XftFont* pub);
void XftUnlockFace(XftFont* pub);
XftFontInfo* XftFontInfoCreate(const FcPattern* pattern);
void XftFontInfoDestroy(XftFontInfo* fi);
FcChar32 XftFontInfoHash(const XftFontInfo* fi);
bool XftFontInfoEqual(const XftFontInfo* a, const XftFontInfo* b);
XftFont* XftFontOpenInfo(FcPattern* pattern, XftFontInfo* fi);
XftFont* XftFontOpenPattern(FcPattern* pattern);
void XftFontClose(XftFont* pub);

/* xftglyphs.c */
void XftFontLoadGlyphs(XftFont* pub, bool need_bitmaps, const FT_UInt* glyphs, int nglyph);
void XftFontUnloadGlyphs(XftFont* pub, const FT_UInt* glyphs, int nglyph);

#define XFT_NMISSING 256

bool XftFontCheckGlyph(XftFont* pub, bool need_bitmaps, FT_UInt glyph, FT_UInt* missing, int* nmissing);
bool XftCharExists(XftFont* pub, FcChar32 ucs4);
FT_UInt XftCharIndex(XftFont* pub, FcChar32 ucs4);

/* xftrender.c */

// eee
void XftGlyphRender1(int op, XRenderColor* src, XftFont* pub, Picture dst, int x, int y, const FT_UInt g, int cw);