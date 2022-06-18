#include "joystick.h"

sbyte	joyx[2], joyy[2];
bool	joyb[2];

void joy_poll(char n)
{
	char	b = ((volatile char *)0xdc00)[n];

	if (!(b & 1))
		joyy[n] = -1;
	else if (!(b & 2))
		joyy[n] = 1;
	else
		joyy[n] = 0;

	if (!(b & 4))
		joyx[n] = -1;
	else if (!(b & 8))
		joyx[n] = 1;
	else
		joyx[n] = 0;

	joyb[n] = (b & 0x10) == 0;
}
