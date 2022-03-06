#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <c64/cia.h>
#include <c64/rasterirq.h>
#include <audio/sidfx.h>
#include <string.h>
#include <stdlib.h>

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xe000;
byte * const TextFont = (byte *)0xe800;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd000;

// Character set 
char charset[2048] = {
	#embed "../resources/hscrollshmupchars.bin"
};

char tcharset[2048] = {
	#embed "../resources/breakoutchars.bin"
};

char tileset[] = {
	#embed "../resources/hscrollshmuptiles.bin"	
};

char tilemap[128 * 5] = {
	#embed "../resources/hscrollshmupmap.bin"		
};

char spriteset[4096] = {
	#embed 4096 0 "../resources/hscrollshmupsprites.bin"
};

char xtileset[16][64];
char xtilemap[144 * 5];
char stars[24];
char xcollision[256];

#pragma align(xtileset, 64);
#pragma align(xcollision, 256)

RIRQCode	bottom, top;

struct Enemy
{
	int		px;
	byte	py;
	sbyte	dx;
	byte	state, pad0, pad1, pad2;
}	enemies[5];

struct Player
{
	int		spx;
	sbyte	vpx;
	sbyte	ax;
	char	aphase;
	char	spy;
	char	fdelay;
}	player;

enum GameState
{
	GS_START,

	GS_READY,			// Getting ready

	GS_PLAYING,
	GS_EXPLODING,

	GS_GAME_OVER
};


struct Game
{
	GameState	state;
	char		count;
	char		edelay;
	char		ecount;
	char		escore;
	char		lives;
}	game;

SIDFX	SIDFXFire[1] = {{
	8000, 1000, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_16 | SID_DKY_114,
	0x40  | SID_DKY_750,
	-80, 0,
	4, 30
}};

SIDFX	SIDFXExplosion[1] = {{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	-20, 0,
	8, 40
}};

SIDFX	SIDFXBigExplosion[3] = {
	{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_168,
	-20, 0,
	4, 0
	},
	{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_168,
	-20, 0,
	10, 0
	},
	{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	-10, 0,
	8, 80
	},	
};


//                          0123456789012345678901234567890123456789
const char StatusText[] = s" Score: 000000  High: 000000   Lives: 0 ";

// initialize status line
void status_init(void)
{
	for(char i=0; i<40; i++)
		Screen[i] = StatusText[i];
}

void status_update(void)
{
	Screen[38] = game.lives + '0';

	// Increment the score from a given digit on
	if (game.escore)
	{
		char	at, val = 1;
		if (game.escore >= 100)
		{
			game.escore -= 100;
			at = 11;
		}
		else if (game.escore >= 10)
		{
			game.escore -= 10;
			at = 12;			
		}
		else
		{
			val = game.escore;
			game.escore = 0;
			at = 13;
		}

		// Loop while there is still score to account for
		while (val)
		{
			// Increment one character
			char	ch = Screen[at] + val;
			val = 0;

			// Check overflow
			if (ch > '9')
			{
				ch -= 10;
				val = 1;
			}

			Screen[at] = ch;

			// Next higher character
			at --;
		}
	}
}

// Reset score and update high score
void score_reset(void)
{
	// Find first digit, where score and highscore differ
	char i = 0;
	while (i < 6 && Screen[i + 8] == Screen[i + 22])
		i++;

	// Check if new score is higher
	if (i < 6 && Screen[i + 8] > Screen[i + 22])
	{
		// If so, copy to highscore
		while (i < 6)
		{
			Screen[i + 22] = Screen[i + 8];
			i++;
		}
	}

	// Clear score
	for(char i=0; i<6; i++)	
		Screen[i + 8] = '0';	
}

// unpack tiles into a fast accessible format
void tiles_unpack(void)
{
	for(char t=0; t<64; t++)
	{
		for(char i=0; i<16; i++)
			xtileset[i][t] = tileset[16 * t + i];
	}

	for(char y=0; y<5; y++)
	{
		for(char x=0; x<144; x++)
		{
			xtilemap[y * 144 + x] = tilemap[y * 128 + (x & 127)];
		}
	}

	for(char i=0; i<160; i+=40)
	{
		for(char j=0; j<3; j++)
			xcollision[i + j] = 1;
	}
}

void tiles_draw0(char * dp, char * tm)
{
	char * ap = dp + 40;
	char * bp = dp + 80;
	char * cp = dp + 120;

	char	q = 0;
	for(char x=0; x<10; x++)
	{
		char	ti = tm[x];

		dp[ q] = xtileset[ 0][ti];
		ap[ q] = xtileset[ 4][ti];
		bp[ q] = xtileset[ 8][ti];
		cp[ q] = xtileset[12][ti];
		q++;

		dp[ q] = xtileset[ 1][ti];
		ap[ q] = xtileset[ 5][ti];
		bp[ q] = xtileset[ 9][ti];
		cp[ q] = xtileset[13][ti];
		q++;

		dp[ q] = xtileset[ 2][ti];
		ap[ q] = xtileset[ 6][ti];
		bp[ q] = xtileset[10][ti];
		cp[ q] = xtileset[14][ti];
		q++;

		dp[ q] = xtileset[ 3][ti];
		ap[ q] = xtileset[ 7][ti];
		bp[ q] = xtileset[11][ti];
		cp[ q] = xtileset[15][ti];
		q++;
	}
}

void tiles_draw3(char * dp, char * tm)
{
	char	ti = tm[0];

	char * ap = dp + 40;
	char * bp = dp + 80;
	char * cp = dp + 120;

	char	q = 0;
	for(char x=1; x<11; x++)
	{
		dp[ q] = xtileset[ 3][ti];
		ap[ q] = xtileset[ 7][ti];
		bp[ q] = xtileset[11][ti];
		cp[ q] = xtileset[15][ti];
		q++;

		ti = tm[x];

		dp[ q] = xtileset[ 0][ti];
		ap[ q] = xtileset[ 4][ti];
		bp[ q] = xtileset[ 8][ti];
		cp[ q] = xtileset[12][ti];
		q++;

		dp[ q] = xtileset[ 1][ti];
		ap[ q] = xtileset[ 5][ti];
		bp[ q] = xtileset[ 9][ti];
		cp[ q] = xtileset[13][ti];
		q++;

		dp[ q] = xtileset[ 2][ti];
		ap[ q] = xtileset[ 6][ti];
		bp[ q] = xtileset[10][ti];
		cp[ q] = xtileset[14][ti];
		q++;
	}
}

void tiles_draw2(char * dp, char * tm)
{
	char	ti = tm[0];
	
	char * ap = dp + 40;
	char * bp = dp + 80;
	char * cp = dp + 120;

	char	q = 0;
	for(char x=1; x<11; x++)
	{
		dp[ q] = xtileset[ 2][ti];
		ap[ q] = xtileset[ 6][ti];
		bp[ q] = xtileset[10][ti];
		cp[ q] = xtileset[14][ti];
		q++;

		dp[ q] = xtileset[ 3][ti];
		ap[ q] = xtileset[ 7][ti];
		bp[ q] = xtileset[11][ti];
		cp[ q] = xtileset[15][ti];
		q++;

		ti = tm[x];

		dp[ q] = xtileset[ 0][ti];
		ap[ q] = xtileset[ 4][ti];
		bp[ q] = xtileset[ 8][ti];
		cp[ q] = xtileset[12][ti];
		q++;

		dp[ q] = xtileset[ 1][ti];
		ap[ q] = xtileset[ 5][ti];
		bp[ q] = xtileset[ 9][ti];
		cp[ q] = xtileset[13][ti];
		q++;
	}
}

void tiles_draw1(char * dp, char * tm)
{
	char	ti = tm[0];

	char * ap = dp + 40;
	char * bp = dp + 80;
	char * cp = dp + 120;

	char	q = 0;
	for(char x=1; x<11; x++)
	{
		dp[ q] = xtileset[ 1][ti];
		ap[ q] = xtileset[ 5][ti];
		bp[ q] = xtileset[ 9][ti];
		cp[ q] = xtileset[13][ti];
		q++;

		dp[ q] = xtileset[ 2][ti];
		ap[ q] = xtileset[ 6][ti];
		bp[ q] = xtileset[10][ti];
		cp[ q] = xtileset[14][ti];
		q++;

		dp[ q] = xtileset[ 3][ti];
		ap[ q] = xtileset[ 7][ti];
		bp[ q] = xtileset[11][ti];
		cp[ q] = xtileset[15][ti];
		q++;

		ti = tm[x];

		dp[ q] = xtileset[ 0][ti];
		ap[ q] = xtileset[ 4][ti];
		bp[ q] = xtileset[ 8][ti];
		cp[ q] = xtileset[12][ti];
		q++;
	}
}


struct Shot
{
	byte		ty, x, ry, n;
	sbyte		dx;
	Shot	*	next;
}	shots[18];

Shot	*	freeShot;

void shot_init(void)
{
	shots[0].next = shots;
	shots[0].ty = 6;

	freeShot = shots + 1;
	for(char i=1; i<17; i++)
		shots[i].next = shots + i + 1;
	shots[17].next = nullptr;
}

inline void shot_draw(char * dp, char i, char xp, char yp)
{
	char		c = dp[xp];

	__assume(i < 20);

	dp[xp] = i | 0xe0;

	char	*	fsp = Font + 8 * c;
	char	*	fdp = (Font + 0xe0 * 8) + 8 * i;

	fdp[0] = fsp[0]; fdp[1] = fsp[1]; fdp[2] = fsp[2]; fdp[3] = fsp[3];
	fdp[4] = fsp[4]; fdp[5] = fsp[5]; fdp[6] = fsp[6]; fdp[7] = fsp[7];

	fdp[yp] = 0x00;
}

void shot_add(int sx, char sy, sbyte dx)
{
	char	py = sy - 14;
	char	gy = py >> 5;
	char	ey = (py >> 3) & 3;
	char	ry = py & 7;

	Shot	*	s = freeShot;
	freeShot = s->next;

	Shot	*	p = shots;
	while (p->next->ty < gy)
		p = p->next;
	s->next = p->next;
	p->next = s;

	s->ty = gy;
	s->ry = ry;
	s->dx = dx;

	if (dx < 0)
	{
		char	x = (sx) >> 3;
		s->n = x - 1;
		s->x = 40 * ey + x;
	}
	else
	{
		char	x = (sx + 24) >> 3;
		s->x = 40 * ey + x;
		s->n = 39 - x;
	}
}

void tiles_draw(unsigned x)
{
//	vic.color_border++;
	vic_waitTop();
	while (vic.raster < 106)
		;
//	vic.color_border--;

	char	xs = 7 - (x & 7);

	rirq_data(&top, 1, VIC_CTRL2_MCM | xs);
	rirq_data(&top, 2, ~(1 << xs));

	x >>= 3;

	char	xl = x >> 2, xr = x & 3;
	char	yl = 0;
	char	ci = 0;

	Shot	*	ps = shots;

	for(int iy=0; iy<5; iy++)
	{
		char	*	dp = Screen + 120 + 160 * iy;
		char	*	cp = Color + 120 + 160 * iy;
		char	*	tp = xtilemap + xl + 144 * iy;

		switch (xr)
		{
		case 0:
			tiles_draw0(dp, tp);
			break;
		case 1:
			tiles_draw1(dp, tp);
			break;
		case 2:
			tiles_draw2(dp, tp);
			break;
		case 3:
			tiles_draw3(dp, tp);
			break;
		default:
			__assume(false);
		}

		char	k = stars[yl + 0] +   0;
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = 0xf8;			
		}

		k = stars[yl + 1];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = 0xf8;			
		}

		k = stars[yl + 2];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = 0xf8;			
		}

		k = stars[yl + 3];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = 0xf8;			
		}

		Shot	*ss = ps->next;

		while (ss->ty == iy)
		{
			ss->x += ss->dx;
			ss->n--;
			shot_draw(dp, ci++, ss->x, ss->ry);
			if (ss->n)
				ps = ss;
			else
			{
				ps->next = ss->next;
				ss->next = freeShot;
				freeShot = ss;
			}
			ss = ps->next;
		}

		yl += 4;
	}

}

void player_init(void)
{
	player.vpx = 0;
	player.ax = 1;
	player.spy = 100;
	player.fdelay = 0;

	spr_set(0, true, 160, 100, 64,          VCOL_BLUE, true, false, false);
	spr_set(7, true, 160, 100, 64 + 16, VCOL_MED_GREY, true, false, false);
	vic.spr_priority = 0x80;
}

void player_control(void)
{
	joy_poll(0);

	if (joyx[0] != 0)
		player.ax = joyx[0];

	player.spy += 2 * joyy[0];
	if (player.spy < 14)
		player.spy = 14;
	else if (player.spy > 14 + 159)
		player.spy = 14 + 159;

	if (player.ax > 0)
	{
		if (player.vpx < 32)
			player.vpx++;

		if (player.aphase > 0 && player.aphase < 16)
			player.aphase--;
		else if (player.aphase >= 16 && player.aphase < 32)
			player.aphase++;
	}
	else if (player.ax < 0)
	{
		if (player.vpx > - 32)
			player.vpx--;

		if (player.aphase >= 0 && player.aphase < 16)
			player.aphase++;
		else if (player.aphase > 16 && player.aphase < 32)
			player.aphase--;

	}

	player.aphase &= 31;

	spr_image(0, 64 + (player.aphase >> 1));
	spr_image(7, 80 + (player.aphase >> 1));

	int	px = 148 - 4 * player.vpx;

	if (player.fdelay)
		player.fdelay--;
	else if (joyb[0])
	{
		if (player.aphase < 4 || player.aphase >= 29)
		{
			shot_add(px, player.spy, 1);
			sidfx_play(0, SIDFXFire, 1);
		}
		else if (player.aphase >= 13 && player.aphase < 20)
		{
			shot_add(px, player.spy, -1);
			sidfx_play(0, SIDFXFire, 1);
		}

		player.fdelay = 6;
	}

	spr_move(0, 24 + px, 50 + player.spy);
	spr_move(7, 32 + px, 58 + player.spy);
}

void player_move()
{
	player.spx += player.vpx >> 2;	
}

void game_init(void)
{
	game.edelay = 80;
	game.ecount = 0;
	game.escore = 0;
	game.lives = 4;
	player.spx = 40;
}

void enemies_move(void)
{
	for(char i=0; i<5; i++)
	{
		if (enemies[i].state)
		{
			enemies[i].px += enemies[i].dx;

			int	rx = enemies[i].px - player.spx;
			if (rx < -192 || rx >= 480)
			{
				enemies[i].state = 0;
				game.ecount--;
				spr_show(2 + i, false);
			}
			else
			{
				spr_move(2 + i, rx + 24, enemies[i].py + 50);

				if (enemies[i].state & 0x80)
				{

				}
				else
				{
					spr_color(2 + i, VCOL_YELLOW);
					spr_image(2 + i, 127 - (enemies[i].state >> 2));
					enemies[i].state--;
					if (enemies[i].state == 0)
					{
						spr_show(2 + i, false);
						game.ecount--;
					}
				}
			}
		}
	}
}

void enemies_spawn(void)
{
	char	u = rand();

	char	e = game.ecount;

	__assume(e < 5);

	sbyte	v = 1 + (u & 3);
	enemies[e].py = 29 + 32 * e;
	enemies[e].dx = player.vpx < 0 ? v : -v;
	enemies[e].px = (player.vpx < 0 ? player.spx - 56 : player.spx + 320) + ((u >> 1) & 31);
	enemies[e].state = 0x80;

	int	rx = enemies[e].px - player.spx;

	spr_set(2 + e, true, rx + 24, enemies[e].py + 50, player.vpx < 0 ? 97 : 96, VCOL_LT_BLUE, true, false, false);
	game.ecount++;
}

void enemies_reset(void)
{
	for(char i=0; i<5; i++)
	{
		spr_show(2 + i, false);
		enemies[i].state = 0x00;
	}

	game.edelay = 80;
	game.ecount = 0;	
}

bool player_check(void)
{	
	char	e = (player.spy - 14) >> 5;
	if (e < 5 && (enemies[e].state & 0x80))
	{
		int	rx = enemies[e].px - player.spx;

		rx -= 148 - 4 * player.vpx;

		if (rx >= - 12 && rx <= 12)
		{
			enemies[e].state = 64;
			return true;
		}
	}

	return false;
}

bool shots_check(void)
{
	Shot	*	ps = shots;
	bool		hit = false;

	for(char i=0; i<5; i++)
	{
		if (enemies[i].state & 0x80)
		{
			sbyte	rx = (enemies[i].px - player.spx) >> 3;
			if (rx >= 0 && rx < 40)
			{
				Shot	*	ss = ps->next;
				while (ss->ty < i)
				{
					ps = ss;
					ss = ps->next;					
				}

				while (ss->ty == i)
				{
					if (xcollision[(char)(ss->x - rx)])
					{
						ps->next = ss->next;
						ss->next = freeShot;
						freeShot = ss;
						enemies[i].state = 64;
						game.escore++;
						hit = true;
						break;
					}						

					ps = ss;
					ss = ps->next;
				}
			}
		}
	}

	return hit;
}

void game_state(GameState state)
{
	// Set new state
	game.state = state;

	switch (state)
	{
	case GS_START:
		game_init();
		game_state(GS_READY);
		break;

	case GS_READY:
		game.count = 120;
		spr_show(0, false);
		spr_show(7, false);

		break;
	case GS_PLAYING:
		player_init();
		break;
	case GS_EXPLODING:
		sidfx_play(0, SIDFXBigExplosion, 3);
		spr_color(0, 7);
		spr_color(7, 8);
		game.count = 64;
		break;
	case GS_GAME_OVER:
		score_reset();
		game.count = 120;
		break;
	}
}

void game_loop()
{
	tiles_draw(player.spx & 4095);

	switch (game.state)
	{
	case GS_START:
		break;
	case GS_READY:
		player.spx -= game.count >> 2;

		if (!--game.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:
		player_control();
		player_move();

		if (game.edelay)
		{
			game.edelay--;
			if (game.edelay < 10 && !(game.edelay & 1))
				enemies_spawn();
		}
		else
		{
			enemies_move();
			if (!game.ecount)
				game.edelay = 64 + (rand() & 63);
		}

		if (shots_check())
			sidfx_play(1, SIDFXExplosion, 1);

		if (player_check())
			game_state(GS_EXPLODING);
		break;

	case GS_EXPLODING:
		spr_image(0, 127 - (game.count >> 2));
		spr_image(7, 127 - (game.count >> 2));

		player_move();
		enemies_move();

		if (!--game.count)
		{
			enemies_reset();
			game.lives--;

			if (game.lives)
				game_state(GS_READY);
			else
				game_state(GS_GAME_OVER);
		}
		break;
	case GS_GAME_OVER:
		if (!--game.count)
			game_state(GS_START);
		break;
	}

//	vic.color_border--;
	sidfx_loop();
//	vic.color_border++;

	status_update();
}


int main(void)
{
	cia_init();

	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);
	memcpy(TextFont, tcharset, 2048);

	char * dp = Font + 248 * 8;
	memset(dp, 0xff, 8);

	memcpy(Sprites, spriteset, 4096);
	mmap_set(MMAP_NO_ROM);

	// bring tiles into better addressed format
	tiles_unpack();

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	// initialize raster IRQ
	rirq_init(true);

	// Set scroll offset and star at top of screen
	rirq_build(&top, 4);
	rirq_write(&top, 0, &vic.memptr, (((unsigned)Screen >> 6) & 0xf0) | (((unsigned)Font >> 10) & 0x0e));
	rirq_write(&top, 1, &vic.ctrl2, VIC_CTRL2_MCM);
	rirq_write(&top, 2, Font + 248 * 8 + 2, 0xff);
	rirq_write(&top, 3, &vic.color_back, VCOL_WHITE);
	rirq_set(0, 58, &top);

	// Switch to text mode for status line and poll joystick at bottom
	rirq_build(&bottom, 3);
	rirq_write(&bottom, 0, &vic.memptr, (((unsigned)Screen >> 6) & 0xf0) | (((unsigned)TextFont >> 10) & 0x0e));
	rirq_write(&bottom, 1, &vic.ctrl2, 0);
	rirq_write(&bottom, 2, &vic.color_back, VCOL_YELLOW);
	rirq_set(1, 250, &bottom);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// Change colors
	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_WHITE;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_MED_GREY;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	// clear screen
	memset(Screen, 0, 1000);
	memset(Color, 0, 80);
	memset(Color + 80, 8, 920);

	sidfx_init();

	sid.fmodevol = 15;

	status_init();

	// initialize background parallax stars
	for(int i=0; i<24; i++)
		stars[i] = rand() % 40 + 40 * (i & 3);

	shot_init();

	game_init();
	player_init();

	// main game loop
	game_state(GS_READY);
	for(;;)
	{
		game_loop();
	}

	return 0;
}
