#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Character set 
char charset[2048] = {
	#embed "../resources/breakoutchars.bin"
};

char spriteset[2048] = {
	#embed "../resources/breakoutsprites.bin"
};

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xd000;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd800;


inline void brick_put(char x, char y, char c)
{
	Screen[40 * y + x +  0] = 128; Color[40 * y + x +  0] = c;
	Screen[40 * y + x +  1] = 129; Color[40 * y + x +  1] = c;
	Screen[40 * y + x +  2] = 130; Color[40 * y + x +  2] = c;

	Screen[40 * y + x + 40] = 131; Color[40 * y + x + 40] = c;
	Screen[40 * y + x + 41] = 132; Color[40 * y + x + 41] = c;
	Screen[40 * y + x + 42] = 133; Color[40 * y + x + 42] = c;
	Screen[40 * y + x + 43] =  97; Color[40 * y + x + 43] = 15;

	Screen[40 * y + x + 81] =  97; Color[40 * y + x + 81] = 15;
	Screen[40 * y + x + 82] =  97; Color[40 * y + x + 82] = 15;
	Screen[40 * y + x + 83] =  97; Color[40 * y + x + 83] = 15;
}

void brick_init(void)
{
	for(char y=0; y<6; y++)
	{
		for(char x=0; x<12; x++)
		{
			brick_put(3 * x + 2, 2 * y + 2, 8 + y);
		}
	}
}

void brick_clr(char x, char y)
{
	char c = Screen[40 * y + x + 0];

	if (c >= 128)
	{
		c &= 7;

		if (c >= 3)
		{
			y --;
			c -= 3;
		}

		x -= c;

		char * cp = Color + 40 * y + x;

		cp[ 0] = 15; 
		cp[ 1] = 15; 
		cp[ 2] = 15; 
		cp[40] = 15; 
		cp[41] = 15; 
		cp[42] = 15; 

		char * sp = Screen + 40 * (y - 1) + (x - 1);

		if (sp[ 0] >= 128)
			sp[41] = 97;
		else
			sp[41] = 96;

		if (sp[ 1] >= 128)
			sp[42] = 97;
		else
			sp[42] = 96;

		if (sp[ 2] >= 128)
			sp[43] = 97;
		else
			sp[43] = 96;

		if (sp[40] >= 128)
			sp[81] = 97;
		else
			sp[81] = 96;

		sp[82] = 96;
		sp[83] = 96;
		if (sp[84] == 97)
			sp[84] = 96;

		if (sp[122] == 97)
			sp[122] = 96;
		if (sp[123] == 97)
			sp[123] = 96;
		if (sp[124] == 97)
			sp[124] = 96;
	}
}

struct Ball
{
	char	index;
	int		sx, sy, vx, vy;	
}

// using 10.6 bit fixed point math

#define BALL_INT(x)			((x) >> 6)
#define BALL_COORD(x, f)	(((x) << 6) + f)

void ball_init(Ball * ball, char index, int sx, int sy, int vx, int vy)
{
	ball->index = index;
	ball->sx = sx;
	ball->sy = sy;
	ball->vx = vx;
	ball->vy = vy;
}

#define	COL_00	1
#define	COL_01	2
#define	COL_10	4
#define	COL_11	8

int paddlex, paddlevx;

void ball_loop(Ball * ball)
{
	ball->sx += ball->vx;
	ball->sy += ball->vy;

	bool	mirrorX = false, mirrorY = false;

	int	ix = BALL_INT(ball->sx), iy = BALL_INT(ball->sy);
	int	px = BALL_INT(paddlex);

	if (ix + 6 > 320 || ix < 0)
		mirrorX = true;
	if (iy < 0)
		mirrorY = true;

	if (iy + 6 > 190 && iy < 190)
	{
		if (ix + 3 >= px && ix + 3 < px + 48)			
		{
			mirrorY = true;
			if (ix < px && ball->vx > 0)
				mirrorX = true;
			else if (ix + 6 >= px + 48 && ball->vx < 0)
				mirrorX = true;

			ball->vx = (ball->vx * 3 + paddlevx) >> 2;
		}
	}


	int	x0 = ix >> 3;
	int y0 = iy >> 3;
	int	x1 = (ix + 6) >> 3;
	int y1 = (iy + 6) >> 3;

	char	col = 0;

	if (y0 >= 0 && Screen[40 * y0 + x0] >= 128) col |= COL_00;
	if (y0 >= 0 && Screen[40 * y0 + x1] >= 128) col |= COL_01;
	if (y1 < 24 && Screen[40 * y1 + x0] >= 128) col |= COL_10;
	if (y1 < 24 && Screen[40 * y1 + x1] >= 128) col |= COL_11;

	if (ball->vx < 0 && ((col & (COL_00 | COL_01)) == COL_00) || ((col & (COL_10 | COL_11)) == COL_10))
		mirrorX = true;
	else if (ball->vx > 0 && ((col & (COL_00 | COL_01)) == COL_01) || ((col & (COL_10 | COL_11)) == COL_11))
		mirrorX = true;

	if (ball->vy < 0 && ((col & (COL_00 | COL_10)) == COL_00) || ((col & (COL_01 | COL_11)) == COL_01))
		mirrorY = true;
	else if (ball->vy > 0 && ((col & (COL_00 | COL_10)) == COL_10) || ((col & (COL_01 | COL_11)) == COL_11))
		mirrorY = true;

	if (col)
	{
		brick_clr(x0, y0);
		brick_clr(x1, y0);
		brick_clr(x0, y1);
		brick_clr(x1, y1);
	}

	if (mirrorY)
	{
		ball->vy = - ball->vy;
		ball->sy += 2 * ball->vy;
	}
	if (mirrorX)
	{
		ball->vx = - ball->vx;
		ball->sx += 2 * ball->vx;
	}
}

void ball_move(Ball * ball)
{
	int	ix = BALL_INT(ball->sx) + 24, iy = BALL_INT(ball->sy) + 50;

	spr_move(2 * ball->index + 2, ix, iy);
	spr_move(2 * ball->index + 3, ix, iy);
}

int main(void)
{
	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);
	memcpy(Sprites, spriteset, 256);
	mmap_set(MMAP_NO_BASIC);

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	// Change colors
	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_WHITE;
	vic.color_back2 = VCOL_DARK_GREY;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	memset(Screen, 96, 1000);
	memset(Color, 15, 1000);

	brick_init();

	spr_set(0, true, 200, 240, 99,  0, false, true, false);
	spr_set(1, true, 200, 240, 98, 15, true,  true, false);

	spr_set(2, true, 200, 200, 97,  0, false, false, false);
	spr_set(3, true, 200, 200, 96, 15, true,  false, false);
#if 0
	spr_set(4, true, 200, 200, 97,  0, false, false, false);
	spr_set(5, true, 200, 200, 96, 15, true,  false, false);

	spr_set(6, true, 200, 200, 97,  0, false, false, false);
	spr_set(7, true, 200, 200, 96, 15, true,  false, false);
#endif

	Ball	balls[3];

	ball_init(balls + 0, 0, BALL_COORD(100, 0), BALL_COORD(180, 0), BALL_COORD(0,  8), BALL_COORD(-1, 48))
//	ball_init(balls + 1, 1, BALL_COORD(100, 0), BALL_COORD(180, 0), BALL_COORD(2, 11), BALL_COORD(-3, 10))
//	ball_init(balls + 2, 2, BALL_COORD(100, 0), BALL_COORD(180, 0), BALL_COORD(1, 37), BALL_COORD( 4, 11))

	for(;;)		
	{
		joy_poll(0);

		if (joyx[0] == 0)
		{
			if (paddlevx < 0)
				paddlevx = (paddlevx + 1) >> 1;
			else
				paddlevx >>= 1;
		}
		else
		{
			paddlevx += joyx[0] * 16;

			if (paddlevx >= 128)
				paddlevx = 128;
			else if (paddlevx < -128)
				paddlevx = -128;
		}

		paddlex += paddlevx;

		spr_set(0, true, 24 + BALL_INT(paddlex), 240, 99,  0, false, true, false);
		spr_set(1, true, 24 + BALL_INT(paddlex), 240, 98, 15, true,  true, false);

		vic.color_border++;
		for(char i=0; i<4; i++)
		{
			for(char j=0; j<1; j++)
				ball_loop(balls + j)
		}
		for(char j=0; j<1; j++)
			ball_move(balls + j);
		vic.color_border--;

		vic_waitFrame();
	}	

	return 0;
}
