#ifndef C64_JOYSTICK_H
#define C64_JOYSTICK_H

extern signed char	joyx[2], joyy[2];
extern bool			joyb[2];

// poll joystick input for joystick 0 or 1 and place
// the x/y direction and the button status into the joyx/y/b
// arrays for

void joy_poll(char n);

#pragma compile("joystick.c")

#endif
