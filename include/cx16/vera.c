#include "vera.h"

void vram_addr(unsigned long addr)
{
	vera.ctrl &= ~VERA_CTRL_ADDRSEL;
	vera.addr = (unsigned)addr;
	vera.addrh = (char)((addr >> 16) & 1) | 0x10;	
}

void vram_put(char data)
{
	vera.data0 = data;
}

void vram_putw(unsigned data)
{
	vera.data0 = data & 0xff;
	vera.data0 = data >> 8;
}

char vram_get(void)
{
	return vera.data0;
}

unsigned vram_getw(void)
{
	unsigned l = vera.data0;
	unsigned h = vera.data0;
	return (h << 8) | l;
}


void vram_put_at(unsigned long addr, char data)
{
	vram_addr(addr);
	vram_put(data);
}

char vram_get_at(unsigned long addr)
{
	vram_addr(addr);
	return vram_get();;
}

void vram_putn(unsigned long addr, const char * data, unsigned size)
{
	vram_addr(addr);
	while(size > 0)
	{
		vram_put(*data++);
		size--;
	}
}

void vram_getn(unsigned long addr, char * data, unsigned size)
{
	vram_addr(addr);

	while(size > 0)
	{
		*data++ = vram_get();
		size--;
	}
}

void vram_fill(unsigned long addr, char data, unsigned size)
{
	vram_addr(addr);

	while(size > 0)
	{
		vram_put(data);
		size--;
	}
}


void vera_spr_set(char spr, unsigned addr32, VERASpriteMode mode8, VERASpriteSize w, VERASpriteSize h, VERASpritePriority z, char pal)
{
	__assume(spr < 128);

	vram_addr(0x1fc00UL + spr * 8);
	vram_putw(addr32 | (mode8 ? 0x8000 : 0x0000));
	vram_putw(0);
	vram_putw(0);
	vram_put(z << 2);
	vram_put((h << 6) | (w << 4) | pal);	
}

void vera_spr_move(char spr, int x, int y)
{
	__assume(spr < 128);

	vram_addr(0x1fc00UL + spr * 8 + 2);
	vram_putw(x);
	vram_putw(y);	
}

void vera_spr_image(char spr, unsigned addr32)
{
	__assume(spr < 128);

	vram_addr(0x1fc00UL + spr * 8);
	vram_put(addr32 & 0xff);
	vera.addrh &= 0x0f;
	char b = vram_get() & 0x80;
	vram_put((addr32 >> 8) | b);
}

void vera_pal_put(char index, unsigned color)
{
	vram_addr(0x1fa00ul + 2 * index);
	vram_putw(color);
}

unsigned vera_pal_get(char index)
{
	vram_addr(0x1fa00ul + 2 * index);
	return vram_getw();
}

void vera_pal_putn(char index, const unsigned * color, unsigned size)
{
	vram_addr(0x1fa00ul + 2 * index);
	while (size > 0)
	{
		vram_putw(*color++);
		size--;
	}
}
