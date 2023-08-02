#include "mouse.h"
#include "sid.h"
#include "cia.h"

sbyte			mouse_dx, mouse_dy;
bool			mouse_lb, mouse_rb;
static char		mouse_px, mouse_py;
static char		mouse_port;

inline signed char dpos(char * old, char mnew)
{
	mnew = (mnew & 0x7f) >> 1;

	char	diff = (mnew - *old) & 0x3f;

	if (diff >= 0x20)
	{
		*old = mnew;
		return diff | 0xe0;
	}
	else if (diff)
	{
		*old = mnew;
		return diff;
	}

	return 0;
}

void mouse_poll(void)
{
	char	b = ((volatile char *)0xdc00)[mouse_port];
	mouse_rb = (b & 0x01) == 0;
	mouse_lb = (b & 0x10) == 0;

	char x = sid.potx, y = sid.poty;

	mouse_dx = dpos(&mouse_px, x);
	mouse_dy = dpos(&mouse_py, y);
}

void mouse_arm(char n)
{	
	mouse_port = n;
	cia1.pra = ciaa_pra_def = n ? 0x7f : 0xbf;	
}

void mouse_init(void)
{
	mouse_arm(1);
	mouse_poll();	
}
