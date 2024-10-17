#include "sprites.h"
#include "rasterirq.h"

static volatile char * vspriteScreen;

#ifdef VSPRITE_BSS
#pragma bss(VSPRITE_BSS)
#endif

void spr_init(char * screen)
{
	vspriteScreen = screen + 0x3f8;
}


void spr_set(char sp, bool show, int xpos, int ypos, char image, char color, bool multi, bool xexpand, bool yexpand)
{
	__assume (sp < 8);

	char	m = 1 << sp;

	if (show)
		vic.spr_enable |= m;
	else
		vic.spr_enable &= ~m;

	if (multi)
		vic.spr_multi |= m;
	else
		vic.spr_multi &= ~m;

	if (xexpand)
		vic.spr_expand_x |= m;
	else
		vic.spr_expand_x &= ~m;

	if (yexpand)
		vic.spr_expand_y |= m;
	else
		vic.spr_expand_y &= ~m;

	vic.spr_pos[sp].y = ypos;
	vic.spr_pos[sp].x = xpos & 0xff;
	if (xpos & 0x100)
		vic.spr_msbx |= m;
	else
		vic.spr_msbx &= ~m;

	vspriteScreen[sp] = image;
	vic.spr_color[sp] = color;
}

void spr_show(char sp, bool show)
{
	__assume (sp < 8);

	if (show)
		vic.spr_enable |= 1 << sp;
	else
		vic.spr_enable &= ~(1 << sp);
}

void spr_move(char sp, int xpos, int ypos)
{
	__assume (sp < 8);

	vic.spr_pos[sp].y = ypos;
	vic.spr_pos[sp].x = xpos & 0xff;
	if (xpos & 0x100)
		vic.spr_msbx |= 1 << sp;
	else
		vic.spr_msbx &= ~(1 << sp);
}

void spr_move16(char sp, int xpos, int ypos)
{
	__assume (sp < 8);

	if (ypos < 0 || ypos >= 256 || xpos < 0 || xpos >= 384)
		xpos = 384;

	vic.spr_pos[sp].y = ypos;
	vic.spr_pos[sp].x = xpos & 0xff;
	if (xpos & 0x100)
		vic.spr_msbx |= 1 << sp;
	else
		vic.spr_msbx &= ~(1 << sp);	
}

int spr_posx(char sp)
{
	return vic.spr_pos[sp].x | ((vic.spr_msbx & (1 << sp)) ? 256 : 0);
}

int spr_posy(char sp)
{
	return vic.spr_pos[sp].y;
}

void spr_image(char sp, char image)
{
	__assume (sp < 8);

	vspriteScreen[sp] = image;
}

void spr_color(char sp, char color)
{
	__assume (sp < 8);

	vic.spr_color[sp] = color;
}

void spr_expand(char sp, bool xexpand, bool yexpand)
{
	__assume (sp < 8);

	char	m = 1 << sp;

	if (xexpand)
		vic.spr_expand_x |= m;
	else
		vic.spr_expand_x &= ~m;

	if (yexpand)
		vic.spr_expand_y |= m;
	else
		vic.spr_expand_y &= ~m;
}

static char	vspriteYLow[VSPRITES_MAX], vspriteXLow[VSPRITES_MAX], vspriteXHigh[VSPRITES_MAX];
static char	vspriteImage[VSPRITES_MAX], vspriteColor[VSPRITES_MAX];

static char	spriteOrder[VSPRITES_MAX], spriteYPos[VSPRITES_MAX + 1];

static RIRQCode	spirq[VSPRITES_MAX - 8], synch;


void vspr_init(char * screen)
{
	vspriteScreen = screen + 0x3f8;

	vic.spr_expand_x = 0;
	vic.spr_expand_y = 0;
	vic.spr_enable = 0xff;

	for(int i=0; i<VSPRITES_MAX - 8; i++)
	{
#ifdef VSPRITE_REVERSE
		int j = (i & 7) ^ 7;
#else
		int j = i & 7;
#endif

		rirq_build(spirq + i, 5);
		rirq_write(spirq + i, 0, &vic.spr_color[j], 0);
		rirq_write(spirq + i, 1, &vic.spr_pos[j].x, 0);
		rirq_write(spirq + i, 2, &vic.spr_pos[j].y, 0);
		rirq_write(spirq + i, 3, &vspriteScreen[j], 0);
		rirq_write(spirq + i, 4, &vic.spr_msbx, 0);
		rirq_set(i, 80 + 4 * i, spirq + i);
	}

	rirq_build(&synch, 0);
	rirq_set(VSPRITES_MAX - 8, 250, &synch);

	for(int i=0; i<VSPRITES_MAX; i++)
	{
		spriteOrder[i] = i;
		vspriteYLow[i] = 0xff;
	}
}

void vspr_screen(char * screen)
{	
	vspriteScreen = screen + 0x3f8;	
	char hi = (unsigned)vspriteScreen >> 8;
	#pragma unroll(8)
	for(int i=0; i<VSPRITES_MAX - 8; i++)
		rirq_addrhi(spirq + i, 3, hi);
}

#pragma native(vspr_init)

void vspr_set(char sp, int xpos, int ypos, char image, char color)
{
	char	yp = (char)ypos;
	if ((ypos & 0xff00 ) || (xpos & 0xfe00))
		yp = 0xff;

	vspriteYLow[sp] = yp;
	vspriteXLow[sp] = (char)xpos;
	vspriteXHigh[sp] = (char)(xpos >> 8);
	vspriteImage[sp] = image;
	vspriteColor[sp] = color;
}

#pragma native(vspr_set)

void vspr_move(char sp, int xpos, int ypos)
{
	char	yp = (char)ypos;
	if ((ypos & 0xff00 ) || (xpos & 0xfe00))
		yp = 0xff;

	vspriteYLow[sp] = yp;
	vspriteXLow[sp] = (char)xpos;
	vspriteXHigh[sp] = (char)(xpos >> 8);
}

void vspr_movex(char sp, int xpos)
{
	vspriteXLow[sp] = (char)xpos;
	vspriteXHigh[sp] = (char)(xpos >> 8);
}

void vspr_movey(char sp, int ypos)
{
	char	yp = (char)ypos;
	if (ypos & 0xff00)
		yp = 0xff;

	vspriteYLow[sp] = yp;
}

void vspr_image(char sp, char image)
{
	vspriteImage[sp] = image;
}

void vspr_color(char sp, char color)
{
	vspriteColor[sp] = color;
}

void vspr_hide(char sp)
{
	vspriteYLow[sp] = 0xff;
}

void vspr_sort(void)
{
	byte rm = vspriteYLow[spriteOrder[0]];
	spriteYPos[1] = rm;

	for(char i = 1; i<VSPRITES_MAX; i++)
	{
		byte ri = spriteOrder[i];
		byte rr = vspriteYLow[ri];
		if (rr < rm)
		{
			byte j = i, rj = rm;
			do 	{
				spriteYPos[j + 1] = rj;
				spriteOrder[j] = spriteOrder[j - 1];
				rj = spriteYPos[j - 1];
				j--;
			} while (rr < rj);
			spriteOrder[j] = ri;
			spriteYPos[j + 1] = rr;
		}
		else
		{
			spriteYPos[i + 1] = rr;
			rm = rr;
		}
	}
}

#pragma native(vspr_sort)

void vspr_update(void)
{
	char	xymask = 0;
	volatile char	*	vsprs = vspriteScreen;
//	char	sypos[VSPRITES_MAX];

#pragma unroll(full)

	for(char ui=0; ui<8; ui++)
	{
		byte ri = spriteOrder[ui];		
#ifdef VSPRITE_REVERSE
		char uj = ui ^ 7;
#else
		char uj = ui;
#endif

		vic.spr_color[uj] = vspriteColor[ri];
		vsprs[uj] = vspriteImage[ri];
#ifdef VSPRITE_REVERSE
		xymask = (xymask << 1) | (vspriteXHigh[ri] & 1);
#else
		xymask = ((unsigned)xymask | (vspriteXHigh[ri] << 8)) >> 1;
#endif
		vic.spr_pos[uj].x = vspriteXLow[ri];
		vic.spr_pos[uj].y = spriteYPos[ui + 1];

//		sypos[ui] = vspriteYLow[ri];
	}

	vic.spr_msbx = xymask;

#pragma unroll(full)
	bool	done = false;

	for(char ti=0; ti<VSPRITES_MAX - 8; ti++)
	{
		if (!done && spriteYPos[ti + 9] < 250)
		{
			byte ri = spriteOrder[ti + 8];
			rirq_move(ti, spriteYPos[ti + 1] + 23);

#ifdef VSPRITE_REVERSE
			char	m = 0x80 >> (ti & 7);
#else
			char	m = 1 << (ti & 7);
#endif

			xymask |= m;
			if (!(vspriteXHigh[ri] & 1))
				xymask ^= m;

			rirq_data(spirq + ti, 2, spriteYPos[ti + 9]);
			rirq_data(spirq + ti, 0, vspriteColor[ri]);
			rirq_data(spirq + ti, 1, vspriteXLow[ri]);
			rirq_data(spirq + ti, 3, vspriteImage[ri]);

			rirq_data(spirq + ti, 4, xymask);
//			spriteYPos[ti + 9] = vspriteYLow[ri];
		}
		else
		{
			rirq_clear(ti);
			done = true;
		}
	}
}

#pragma native(vspr_update)
