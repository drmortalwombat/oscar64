#ifndef SPRITES_H
#define SPRITES_H

#include "vic.h"


void spr_init(char * screen);

void spr_set(char sp, bool show, int xpos, int ypos, char image, char color, bool multi, bool xexpand, bool yexpand);

inline void spr_show(char sp, bool show);

inline void spr_move(char sp, int xpos, int ypos);

inline void spr_image(char sp, char image);

inline void spr_color(char sp, char color);


void vspr_init(char * screen);


void vspr_set(char sp, int xpos, int ypos, char image, char color);

inline void vspr_move(char sp, int xpos, int ypos);

inline void vspr_image(char sp, char image);

inline void vspr_color(char sp, char color);

inline void vspr_hide(char sp);

void vspr_sort(void);

void vspr_update(void);


#pragma compile("sprites.c")

#endif
