#pragma once

#include <stdarg.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fontconfig/fontconfig.h>

#include <X11/extensions/Xrender.h>

#include "../common.h"

#define XFT_MAX_GLYPH_MEMORY	"maxglyphmemory"
#define XFT_MAX_UNREF_FONTS	"maxunreffonts"

extern FT_Library	ft_library;

// Glyphs are stored in this structure
typedef struct XftGlyph {
	XGlyphInfo metrics;
	unsigned long glyph_memory;
	Picture picture;
} XftGlyph;

// This structure holds the data extracted from a pattern
// needed to create a unique font object.
typedef struct XftFontInfo {
	// Hash value (not include in hash value computation)
	//FcChar32	hash;
	struct XftFtFile* file; // face source
	// Rendering options
	FT_F26Dot6 xsize, ysize; // pixel size
	
	// these need to be FcBool because some functions output using pointers to these
	FcBool antialias;	// doing antialiasing
	FcBool embolden; // force emboldening
	FcBool color; // contains color glyphs
	int rgba; // subpixel order
	int lcd_filter; // lcd filter
	FT_Matrix matrix;	// glyph transformation matrix
	FcBool transform;	// non-identify matrix?
	FT_Int load_flags; // glyph load flags
	// Internal fields
	int spacing;
	FcBool minspace;
	int char_width;
} XftFontInfo;

typedef struct XftFont {
	int ascent, descent, height;
	int max_advance_width;
	FcCharSet* charset;
	FcPattern* pattern;
	
	struct XftFont* next; // all fonts on display
	XftFontInfo info;	// Data from pattern
	int ref;	// reference count
	// Per-glyph information, indexed by glyph ID
	// This array follows the font in memory
	XftGlyph** glyphs;
	int num_glyphs; // size of glyphs/bitmaps arrays
	// Hash table to get from Unicode value to glyph ID
	// This array follows the glyphs in memory
	struct XftUcsHash* hash_table;
	int hash_value;
	int rehash_value;
	// X specific fields
	GlyphSet glyphset; // Render glyphset
	XRenderPictFormat* format;	// Render format for glyphs
	// Glyph memory management fields
	unsigned long glyph_memory;
	unsigned long max_glyph_memory;
} XftFont;

typedef struct XftDraw XftDraw;

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
XftDraw* XftDrawCreate(Px w, Px h);

Drawable XftDrawDrawable(XftDraw* draw);

void XftDrawDestroy(XftDraw* draw);

Picture XftDrawPicture(XftDraw* draw);

Picture XftDrawSrcPicture(const XRenderColor color);

void XftDrawRect(XftDraw* draw, const XRenderColor color, Px x, Px y, Px width, Px height);

void XftDrawPut(XftDraw* draw, Px x, Px y, Px w, Px h, Px dx, Px dy);

/* xftextent.c */

void XftGlyphExtents(XftFont* pub, const FT_UInt* glyphs, int nglyphs, XGlyphInfo* extents);
void XftTextExtents32(XftFont* pub, const FcChar32* string, int len, XGlyphInfo* extents);

/* xftfreetype.c */
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
void XftGlyphRender1(int op, XRenderColor col, XftFont* font, Picture dst, float x, int y, FT_UInt g);
