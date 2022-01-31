#ifndef MCBITMAP_H
#define MCBITMAP_H

#include "bitmap.h"


extern char MixedColors[4][4][8];


// Set a single pixel
void bmmc_put(const Bitmap * bm, int x, int y, char c);


// Get the state of a single pixel
char bmmc_get(const Bitmap * bm, int x, int y);


// Draw an unclipped line using an eight bit pattern
void bmmcu_line(const Bitmap * bm, int x0, int y0, int x1, int y1, char color);

// Draw a clipped line using an eight bit pattern
void bmmc_line(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, char color);


inline void bmmc_scan_fill(int left, int right, char * lp, int x0, int x1, char pat);

void bmmcu_circle(const Bitmap * bm, int x, int y, char r, char color);

void bmmc_circle(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, char color);

// Fill a circle with center x/y and radius r and the 8x8 pattern pat.
void bmmc_circle_fill(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, const char * pat);

// Fill a trapezoid with horizontal top and bottom, top left is in x0, top right in x1
// dx0 and dx1 are the horizontal delta for each line. Coordinates are in 16.16 fixed point
// numbers.  y0 and y1 are vertical coordinates in pixel.
void bmmc_trapezoid_fill(const Bitmap * bm, const ClipRect * clip, long x0, long x1, long dx0, long dx1, int y0, int y1, const char * pat)

// Fill a triangle with a pattern, coordinate pairs x0/y0, x1/y1 and x2/y2 are in pixel
void bmmc_triangle_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, const char * pat);

// Fill a quad with a pattern, coordinate pairs are in pixel
void bmmc_quad_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, const char * pat);


// Fill a convex polygon with a pattern, coordinate pairs x[]/y[] are in pixel
void bmmc_polygon_fill(const Bitmap * bm, const ClipRect * clip, int * x, int * y, char num, const char * pat);

// Fill an arbitrary polygon with a pattern, coordinate pairs x[]/y[] are in pixel, maximum size is
// sixteen vertices
void bmmc_polygon_nc_fill(const Bitmap * bm, const ClipRect * clip, int * x, int * y, char num, const char * pat);

inline void bmmcu_rect_fill(const Bitmap * dbm, int dx, int dy, int w, int h, char color);

// Unclipped rectangle pattern fill
inline void bmmcu_rect_pattern(const Bitmap * dbm, int dx, int dy, int w, int h, const char * pattern);

// Unclipped rectangle copy
inline void bmmcu_rect_copy(const Bitmap * dbm, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h);


// Clipped rectangle fill
inline void bmmc_rect_fill(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, char color);

// Clipped rectangle pattern fill
inline void bmmc_rect_pattern(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, const char * pattern);

// Clipped rectangle copy
inline void bmmc_rect_copy(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h);


void bmmc_flood_fill(const Bitmap * bm, const ClipRect * clip, int x, int y, char color);

#pragma compile("mcbitmap.c")

#endif
