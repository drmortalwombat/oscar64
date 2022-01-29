#ifndef BITMAP_H
#define BITMAP_H

#include <c64/types.h>

struct Bitmap
{
	char	*	data, * rdata;
	char		cwidth;
	char		cheight;
	unsigned	width;
};

struct ClipRect
{
	int			left, top, right, bottom;
};

#define BLIT_OP				0x03

#define BLIT_AND			0x01
#define BLIT_ORA			0x02
#define BLIT_EOR			0x03

#define BLIT_IMM			0x04
#define BLIT_SRC			0x08
#define BLIT_DST			0x10
#define BLIT_PATTERN		0x20

#define BLIT_INVERT			0x40


enum BlitOp
{
	BLTOP_SET             = BLIT_IMM | BLIT_INVERT,
	BLTOP_RESET           = BLIT_IMM,
	BLTOP_NOT             = BLIT_DST | BLIT_IMM | BLIT_INVERT | BLIT_EOR,

	BLTOP_XOR             = BLIT_SRC | BLIT_DST | BLIT_EOR,
	BLTOP_OR              = BLIT_SRC | BLIT_DST | BLIT_ORA,
	BLTOP_AND             = BLIT_SRC | BLIT_DST | BLIT_AND,
	BLTOP_AND_NOT         = BLIT_SRC | BLIT_DST | BLIT_INVERT | BLIT_AND,

	BLTOP_COPY            = BLIT_SRC,
	BLTOP_NCOPY           = BLIT_SRC | BLIT_INVERT,

	BLTOP_PATTERN         = BLIT_PATTERN,
	BLTOP_PATTERN_AND_SRC = BLIT_SRC | BLIT_PATTERN | BLIT_AND
};

enum LineOp
{
	LINOP_SET,
	LINOP_OR,
	LINOP_AND,
	LINOP_XOR
};

extern char NineShadesOfGrey[9][8];

// Fast unsigned integer square root
unsigned bm_usqrt(unsigned n);

// Initialize a bitmap structure, size is given in char cells (8x8 pixel)
void bm_init(Bitmap * bm, char * data, char cw, char ch);

// Initialize a bitmap structure with allocated memory, size is given in char cells (8x8 pixel)
void bm_alloc(Bitmap * bm, char cw, char ch);

// Free the memory of a bitmap
void bm_free(Bitmap * bm);

// Fill a bitmap with the data byte
void bm_fill(Bitmap * bm, char data);


void bm_scan_fill(int left, int right, char * lp, int x0, int x1, char pat);

// Fill a circle with center x/y and radius r and the 8x8 pattern pat.
void bm_circle_fill(Bitmap * bm, ClipRect * clip, int x, int y, char r, const char * pat);

// Fill a trapezoid with horizontal top and bottom, top left is in x0, top right in x1
// dx0 and dx1 are the horizontal delta for each line. Coordinates are in 16.16 fixed point
// numbers.  y0 and y1 are vertical coordinates in pixel.
void bm_trapezoid_fill(Bitmap * bm, ClipRect * clip, long x0, long x1, long dx0, long dx1, int y0, int y1, const char * pat)

// Fill a triangle with a pattern, coordinate pairs x0/y0, x1/y1 and x2/y2 are in pixel
void bm_triangle_fill(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, const char * pat);

// Fill a quad with a pattern, coordinate pairs are in pixel
void bm_quad_fill(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, const char * pat);

// Fill a convex polygon with a pattern, coordinate pairs x[]/y[] are in pixel
void bm_polygon_fill(Bitmap * bm, ClipRect * clip, int * x, int * y, char num, const char * pat);

// Fill an arbitrary polygon with a pattern, coordinate pairs x[]/y[] are in pixel, maximum size is
// sixteen vertices
void bm_polygon_nc_fill(Bitmap * bm, ClipRect * clip, int * x, int * y, char num, const char * pat);

// Set a single pixel
inline void bm_set(Bitmap * bm, int x, int y);

// Clear a single pixel
inline void bm_clr(Bitmap * bm, int x, int y);

// Get the state of a single pixel
inline bool bm_get(Bitmap * bm, int x, int y);

// Set or clear a single pixel
inline void bm_put(Bitmap * bm, int x, int y, bool c);

// Draw an unclipped line using an eight bit pattern
void bmu_line(Bitmap * bm, int x0, int y0, int x1, int y1, char pattern, LineOp op);

// Draw a clipped line using an eight bit pattern
void bm_line(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, char pattern, LineOp op);

// Unclipped bit blit
void bmu_bitblit(Bitmap * dbm, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op);

// Unclipped rectangle fill
inline void bmu_rect_fill(Bitmap * dbm, int dx, int dy, int w, int h);

// Unclipped rectangle clear
inline void bmu_rect_clear(Bitmap * dbm, int dx, int dy, int w, int h);

// Unclipped rectangle pattern fill
inline void bmu_rect_pattern(Bitmap * dbm, int dx, int dy, int w, int h, const char * pattern);

// Unclipped rectangle copy
inline void bmu_rect_copy(Bitmap * dbm, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h);


// Clipped bit blit
void bm_bitblit(Bitmap * dbm, ClipRect * clip, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op);

// Clipped rectangle fill
inline void bm_rect_fill(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h);

// Clipped rectangle clear
inline void bm_rect_clear(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h);

// Clipped rectangle pattern fill
inline void bm_rect_pattern(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h, const char * pattern);

// Clipped rectangle copy
inline void bm_rect_copy(Bitmap * dbm, ClipRect * clip, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h);


// Unclipped text rendering
int bmu_text(Bitmap * bm, const char * str, char len);

// Calculate size of a char range
int bmu_text_size(const char * str, char len);

// Unclipped text output to an arbitrary location using a bit blit
int bmu_put_chars(Bitmap * bm, int x, int y, const char * str, char len, BlitOp op);

// Clipped text output to an arbitrary location using a bit blit
int bm_put_chars(Bitmap * bm, ClipRect * clip, int x, int y, const char * str, char len, BlitOp op);

// Clipped text output of a zero terminated string to an arbitrary location using a bit blit
inline int bm_put_string(Bitmap * bm, ClipRect * clip, int x, int y, const char * str, BlitOp op);


// Linear transformation of a source bitmap rectangle to a destination rectangle
int bm_transform(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h, Bitmap * sbm, int sx, int sy, int dxx, int dxy, int dyx, int dyy);


#pragma compile("bitmap.c")

#endif
