#include "vic.h"
#include "cia.h"

void vic_setbank(char bank)
{
	cia2.pra = (cia2.pra & 0xfc) | (bank ^ 0x03);
}

void vic_sprxy(byte s, int x, int y)
{
	vic.spr_pos[s].y = y;
	vic.spr_pos[s].x = x & 0xff;
	if (x & 0x100)
		vic.spr_msbx |= 1 << s;
	else
		vic.spr_msbx &= ~(1 << s);
}
