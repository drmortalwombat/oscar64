#include "sprites.h"
#include "rasterirq.h"

static char * vspriteScreen;


void spr_init(char * screen)
{
	vspriteScreen = screen + 0x3f8;
}


void spr_set(char sp, bool show, int xpos, int ypos, char image, char color, bool multi, bool xexpand, bool yexpand)
{
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
	if (show)
		vic.spr_enable |= 1 << sp;
	else
		vic.spr_enable &= ~(1 << sp);
}

void spr_move(char sp, int xpos, int ypos)
{
	vic.spr_pos[sp].y = ypos;
	vic.spr_pos[sp].x = xpos & 0xff;
	if (xpos & 0x100)
		vic.spr_msbx |= 1 << sp;
	else
		vic.spr_msbx &= ~(1 << sp);
}

void spr_image(char sp, char image)
{
	vspriteScreen[sp] = image;
}

void spr_color(char sp, char color)
{
	vic.spr_color[sp] = color;
}


#define NUM_SPRITES		16

static char	vspriteYLow[NUM_SPRITES], vspriteXLow[NUM_SPRITES], vspriteXHigh[NUM_SPRITES];
static char	vspriteImage[NUM_SPRITES], vspriteColor[NUM_SPRITES];

static char	spriteOrder[16], spriteYPos[17];

static RIRQCode	spirq[8], synch;


void vspr_init(char * screen)
{
	vspriteScreen = screen + 0x3f8;

	vic.spr_expand_x = 0;
	vic.spr_expand_y = 0;
	vic.spr_enable = 0xff;

	for(int i=0; i<8; i++)
	{
		rirq_build(spirq + i, 5);
		rirq_write(spirq + i, 0, &vic.spr_color[i], 0);
		rirq_write(spirq + i, 1, &vic.spr_pos[i].x, 0);
		rirq_write(spirq + i, 2, &vic.spr_pos[i].y, 0);
		rirq_write(spirq + i, 3, &vspriteScreen[i], 0);
		rirq_write(spirq + i, 4, &vic.spr_msbx, 0);
		rirq_set(i, 80 + 8 * i, spirq + i);
	}

	rirq_build(&synch, 0);
	rirq_set(8, 250, &synch);

	for(int i=0; i<16; i++)
	{
		spriteOrder[i] = i;
		vspriteYLow[i] = 0xff;
	}
}

#pragma native(vspr_init)

void vspr_set(char sp, int xpos, int ypos, char image, char color)
{
	vspriteYLow[sp] = (char)ypos;
	vspriteXLow[sp] = (char)xpos;
	vspriteXHigh[sp] = (char)(xpos >> 8);
	vspriteImage[sp] = image;
	vspriteColor[sp] = color;
	if ((ypos & 0xff00 ) || (xpos & 0xfe00))
		vspriteYLow[sp] = 0xff;
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
	spriteYPos[1] = vspriteYLow[spriteOrder[0]];

	for(char i = 1; i<16; i++)
	{
		byte ri = spriteOrder[i];
		byte rr = vspriteYLow[ri];
		byte j = i, rj = spriteYPos[j];
		while (rr < rj)
		{
			spriteYPos[j + 1] = rj;
			spriteOrder[j] = spriteOrder[j - 1];
			rj = spriteYPos[j - 1];
			j--;
		}
		spriteOrder[j] = ri;
		spriteYPos[j + 1] = rr;
	}
}

#pragma native(vspr_sort)

void vspr_update(void)
{
	char	xymask = 0;
	char	*	vsprs = vspriteScreen;

	for(char ui=0; ui<8; ui++)
	{
		byte ri = spriteOrder[ui];

		vic.spr_color[ui] = vspriteColor[ri];
		vsprs[ui] = vspriteImage[ri];
		xymask = ((unsigned)xymask | (vspriteXHigh[ri] << 8)) >> 1;
		vic.spr_pos[ui].x = vspriteXLow[ri];
		vic.spr_pos[ui].y = vspriteYLow[ri];
	}

	vic.spr_msbx = xymask;

	if (spriteYPos[8] < 230)
	{
		char	m = 1;
		for(char ti=0; ti<8; ti++)
		{

			byte ri = spriteOrder[ti + 8];

			xymask |= m;
			if (!vspriteXHigh[ri])
				xymask ^= m;

			rirq_data(spirq + ti, 0, vspriteColor[ri]);
			rirq_data(spirq + ti, 1, vspriteXLow[ri]);
			rirq_data(spirq + ti, 2, vspriteYLow[ri]);
			rirq_data(spirq + ti, 3, vspriteImage[ri]);

			rirq_data(spirq + ti, 4, xymask);
			rirq_move(ti, spriteYPos[ti + 1] + 21);

			m <<= 1;
		}
	}
	else
	{
		for(char ti=0; ti<8; ti++)
			rirq_clear(ti);
	}
}

#pragma native(vspr_update)
