#ifndef C64_MOUSE_H
#define C64_MOUSE_H

#include "types.h"

extern sbyte	mouse_dx, mouse_dy;
extern bool		mouse_lb, mouse_rb;


void mouse_init(void);

// arm the potentiometer input for the selected mouse input
// needs ~4ms to stabilize

void mouse_arm(char n);

// poll mouse input for selected mouse, but the relative
// movement into mouse_dx/dy and the button state into
// mouse_lb/mouse_rb

void mouse_poll(void);

#pragma compile("mouse.c")

#endif

