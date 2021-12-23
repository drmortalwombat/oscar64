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

extern char NineShadesOfGrey[9][8];


void bm_init(Bitmap * bm, char * data, char cw, char ch);

void bm_alloc(Bitmap * bm, char cw, char ch);

void bm_free(Bitmap * bm);

void bm_fill(Bitmap * bm, char data);


void bm_scan_fill(int left, int right, char * lp, int x0, int x1, char pat);

void bm_circle_fill(Bitmap * bm, ClipRect * clip, int x, int y, char r, const char * pat);

void bm_trapezoid_fill(Bitmap * bm, ClipRect * clip, long x0, long x1, long dx0, long dx1, int y0, int y1, const char * pat)

void bm_triangle_fill(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, const char * pat);

void bm_quad_fill(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, const char * pat);

void bm_polygon_fill(Bitmap * bm, ClipRect * clip, int * x, int * y, char num, const char * pat);

void bm_polygon_nc_fill(Bitmap * bm, ClipRect * clip, int * x, int * y, char num, const char * pat);

inline void bm_set(Bitmap * bm, int x, int y);
inline void bm_clr(Bitmap * bm, int x, int y);
inline bool bm_get(Bitmap * bm, int x, int y);
inline void bm_put(Bitmap * bm, int x, int y, bool c);


void bm_line(Bitmap * bm, int x0, int y0, int x1, int y1, char pattern);

void bm_line_clipped(Bitmap * bm, ClipRect * clip, int x0, int y0, int x1, int y1, char pattern);


void bm_bitblit(Bitmap * dbm, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op);

inline void bm_rect_fill(Bitmap * dbm, int dx, int dy, int w, int h);

inline void bm_rect_clear(Bitmap * dbm, int dx, int dy, int w, int h);

inline void bm_rect_pattern(Bitmap * dbm, int dx, int dy, int w, int h, const char * pattern);

inline void bm_rect_copy(Bitmap * dbm, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h);


void bm_bitblit_clipped(Bitmap * dbm, ClipRect * clip, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op);

inline void bm_rect_fill_clipped(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h);

inline void bm_rect_clear_clipped(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h);

inline void bm_rect_pattern_clipped(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h, const char * pattern);

inline void bm_rect_copy_clipped(Bitmap * dbm, ClipRect * clip, int dx, int dy, Bitmap * sbm, int sx, int sy, int w, int h);

int bm_text(Bitmap * bm, const char * str, char len);

int bm_text_size(const char * str, char len);

int bm_put_chars(Bitmap * bm, int x, int y, const char * str, char len, BlitOp op);

int bm_put_chars_clipped(Bitmap * bm, ClipRect * clip, int x, int y, const char * str, char len, BlitOp op);


int bm_transform(Bitmap * dbm, ClipRect * clip, int dx, int dy, int w, int h, Bitmap * sbm, int sx, int sy, int dxx, int dxy, int dyx, int dyy);

#pragma compile("bitmap.c")

#endif
