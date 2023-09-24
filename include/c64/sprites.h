#ifndef SPRITES_H
#define SPRITES_H

#include "vic.h"

// initialize non virtualized sprite system, using only the eight hardware sprites

void spr_init(char * screen);

// set one sprite with the given attributes

void spr_set(char sp, bool show, int xpos, int ypos, char image, char color, bool multi, bool xexpand, bool yexpand);

// show or hide a sprite

inline void spr_show(char sp, bool show);

// move a sprite to the given position, only uses 8 bit y and 9 bit x

inline void spr_move(char sp, int xpos, int ypos);

// get current x position of sprite

inline int spr_posx(char sp);

// get current y position of sprite

inline int spr_posy(char sp);

// move a sprite to the given position, only uses 16 bit y and 16 bit x,
// moves the sprite to a zero y position if offscreen
void spr_move16(char sp, int xpos, int ypos);

// change the image of a sprite

inline void spr_image(char sp, char image);

// change the color of a sprite

inline void spr_color(char sp, char color);

// change the image of a sprite

inline void spr_expand(char sp, bool xexpand, bool yexpand);

// The virtual sprite system works with the rasterirq library to multiplex
// 16 virtual sprites onto the actual eight hardware sprites.  It uses the slots
// 0 to 8 of the rasterirq library to switch the sprites mid screen.  The
// application has to race the beam and call at least the vspr_update every
// bottom of the frame to reset the top eight sprites.
//
// A usual frame would look like this:
//
// - off screen game code
// vspr_sort();
// - more game code
// rirq_wait();
// vspr_update();
// - more raster irq stuff
// rirq_sort();
//

#ifndef VSPRITES_MAX
#define VSPRITES_MAX	16
#endif

// initialize the virtual (multiplexed) sprite system, offering 16 sprites

void vspr_init(char * screen);

// set one sprite with the given attribute

void vspr_set(char sp, int xpos, int ypos, char image, char color);

// move a virtual sprite

inline void vspr_move(char sp, int xpos, int ypos);

// change the image of a virtual sprite

inline void vspr_image(char sp, char image);

// change the color of a virtual sprite

inline void vspr_color(char sp, char color);

// hide a virtual sprite, show again by moving it into the visual range

inline void vspr_hide(char sp);


// sort the virtual sprites by their y-position

void vspr_sort(void);

// update the virtual sprites.  Must be called every frame before sorting
// the raster irq list.

void vspr_update(void);


#pragma compile("sprites.c")

#endif
