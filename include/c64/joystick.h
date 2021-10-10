#ifndef C64_JOYSTICK_H
#define C64_JOYSTICK_H

extern signed char	joyx[2], joyy[2];
extern bool			joyb[2];

void joy_poll(char n);

#pragma compile("joystick.c")

#endif
