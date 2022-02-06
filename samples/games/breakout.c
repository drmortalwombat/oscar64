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
	char * cp = Color + 40 * y + x;

	cp[ 0] = c;
	cp[ 1] = c;
	cp[ 2] = c;
	cp[40] = c;
	cp[41] = c;
	cp[42] = c;
	cp[81] = 15;
	cp[82] = 15;
	cp[83] = 15;

	char * sp = Screen + 40 * y + x;

	sp[ 0] = 128; 
	sp[ 1] = 129; 
	sp[ 2] = 130; 

	sp[40] = 131; 
	sp[41] = 132; 
	sp[42] = 133; 
	sp[43] =  97; 

	sp[81] =  97; 
	sp[82] =  97; 
	sp[83] =  97; 
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
	__assume(x < 40 && y < 25);

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
	bool	active;
	int		sx, sy, vx, vy;	
}

// using 10.6 bit fixed point math

#define BALL_INT(x)			((x) >> 6)
#define BALL_COORD(x, f)	(((x) << 6) + f)

void ball_init(Ball * ball, char index, int sx, int sy, int vx, int vy)
{
	ball->index = index;
	ball->active = true;
	ball->sx = sx;
	ball->sy = sy;
	ball->vx = vx;
	ball->vy = vy;

	spr_set(2 * index + 2, true, 200, 200, 97,  0, false, false, false);
	spr_set(2 * index + 3, true, 200, 200, 96, 15, true,  false, false);
}

void ball_lost(Ball * ball)
{
	ball->active = false;

	spr_show(2 * ball->index + 2, false);
	spr_show(2 * ball->index + 3, false);

	return;
}

#define	COL_00	1
#define	COL_01	2
#define	COL_10	4
#define	COL_11	8

int paddlex, paddlevx;

void ball_loop(Ball * ball)
{
	if (!ball->active)
		return;

	ball->sx += ball->vx;
	ball->sy += ball->vy;

	bool	mirrorX = false, mirrorY = false;

	int	ix = BALL_INT(ball->sx), iy = BALL_INT(ball->sy);
	int	px = BALL_INT(paddlex);

	if (ix + 6 > 320 || ix < 0)
		mirrorX = true;

	if (iy < 0)
		mirrorY = true;
	else if (iy >= 200)
	{
		ball_lost(ball);
		return;
	}

	if (iy + 6 > 190 && iy < 190)
	{
		if (ix + 3 >= px && ix + 3 < px + 48)			
		{
			mirrorY = true;
			if (ix < px && ball->vx > 0)
				mirrorX = true;
			else if (ix + 6 >= px + 48 && ball->vx < 0)
				mirrorX = true;
			else
				ball->vx = (ball->vx * 12 + paddlevx + 8) >> 4;
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
		ball->sy += ball->vy;
	}
	if (mirrorX)
	{
		ball->vx = - ball->vx;
		ball->sx += ball->vx;
	}
}

void ball_move(Ball * ball)
{
	int	ix = BALL_INT(ball->sx) + 24, iy = BALL_INT(ball->sy) + 50;

	spr_move(2 * ball->index + 2, ix, iy);
	spr_move(2 * ball->index + 3, ix, iy);
}

enum GameState
{
	GS_READY,			// Getting ready

	GS_BALL_LOCKED,		// The ball is locked on the paddle
	GS_PLAYING,			// Playing the game
	GS_BALL_DROPPED,	// The last ball has been dropped

	GS_GAME_OVER
};

struct Game
{
	GameState		state;
	Ball			balls[3];
	char			count;

}	TheGame;

void paddle_init(void)
{
	paddlevx = 0;
	paddlex = BALL_COORD(160, 0);

	spr_set(0, true, 24 + BALL_INT(paddlex), 240, 99,  0, false, true, false);
	spr_set(1, true, 24 + BALL_INT(paddlex), 240, 98, 15, true,  true, false);	
}

void paddle_control(void)
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
		paddlevx += joyx[0] * 8;

		if (paddlevx >= 256)
			paddlevx = 256;
		else if (paddlevx < -256)
			paddlevx = -256;
	}

	paddlex += paddlevx;
	if (paddlex < BALL_COORD(-4, 0) || paddlex > BALL_COORD(320 - 48 + 4, 0))
	{
		paddlevx = -paddlevx;
		paddlex += paddlevx
	}
}

void paddle_move(void)
{
	spr_move(0, 24 + BALL_INT(paddlex), 240);
	spr_move(1, 24 + BALL_INT(paddlex), 240);
}

void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	switch (state)
	{
	case GS_READY:
		brick_init();
		paddle_init();
		TheGame.count = 120;
		break;

	case GS_BALL_LOCKED:
		ball_init(TheGame.balls + 0, 0, paddlex + BALL_COORD(12, 0), BALL_COORD(180, 0), BALL_COORD(0,  0), BALL_COORD(0, 0))
		break;		

	case GS_PLAYING:
		TheGame.balls[0].vy = BALL_COORD(-1, 0);
		TheGame.balls[0].vx = paddlevx >> 2;
		break;

	case GS_BALL_DROPPED:
		TheGame.count = 60;
		break;

	case GS_GAME_OVER:
		TheGame.count = 120;
		break;
	}
}

void game_loop()
{
	switch (TheGame.state)
	{
	case GS_READY:
		if (!--TheGame.count)
			game_state(GS_BALL_LOCKED);	
		break;
	case GS_BALL_LOCKED:
		paddle_control();
		TheGame.balls[0].sx = paddlex + BALL_COORD(22, 0);
		if (joyb[0])
			game_state(GS_PLAYING);	
		break;
	case GS_PLAYING:
		paddle_control();
		vic.color_border++;
		for(char i=0; i<4; i++)
		{
			for(char j=0; j<3; j++)
				ball_loop(TheGame.balls + j)
		}
		vic.color_border--;
		if (!(TheGame.balls[0].active || TheGame.balls[1].active || TheGame.balls[2].active))
			game_state(GS_BALL_DROPPED);
		break;
	case GS_BALL_DROPPED:
		if (!--TheGame.count)
			game_state(GS_BALL_LOCKED);	
		break;
	case GS_GAME_OVER:
		if (!--TheGame.count)
			game_state(GS_READY);	
		break;
	}
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

	game_state(GS_READY);

	for(;;)		
	{
		game_loop();
		vic_waitFrame();

		for(char j=0; j<1; j++)
			ball_move(TheGame.balls + j);
		paddle_move();

	}	

	return 0;
}
