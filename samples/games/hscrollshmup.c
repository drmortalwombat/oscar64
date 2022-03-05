#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <c64/cia.h>
#include <c64/rasterirq.h>
#include <string.h>
#include <stdlib.h>

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xe000;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd000;

// Character set 
char charset[2048] = {
	#embed "../../../assets/uridium1 - Chars.bin"
};

char tileset[] = {
	#embed "../../../assets/uridium1 - Tiles.bin"	
};

char tilemap[128 * 5] = {
	#embed "../../../assets/uridium1 - Map (128x5).bin"		
};

char spriteset[4096] = {
	#embed 4096 0 "../../../assets/uridium1 - Sprites.bin"
};

char xtileset[16][64];
char xtilemap[144 * 5];
char stars[24];
char xcollision[256];

#pragma align(xtileset, 64);
#pragma align(xcollision, 256)

RIRQCode	bottom, top;


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

void shot_add(int dx, int sy)
{
	char	py = sy - 6;
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
	if (dx < 0)
	{
		s->dx = -1;
		char	x = (148 - 4 * dx) >> 3;
		s->n = x - 1;
		s->x = 40 * ey + x;
	}
	else
	{
		s->dx = 1;
		char	x = (156 - 4 * dx) >> 3;
		s->x = 40 * ey + x;
		s->n = 39 - x;
	}
}

void tiles_draw(unsigned x)
{
	char	xs = 7 - (x & 7);

	rirq_data(&top, 0, VIC_CTRL2_MCM | xs);
	rirq_data(&top, 1, ~(1 << xs));

	x >>= 3;

	char	xl = x >> 2, xr = x & 3;
	char	yl = 0;
	char	ci = 0;

	Shot	*	ps = shots;

	for(int iy=0; iy<5; iy++)
	{
		char	*	dp = Screen + 80 + 160 * iy;
		char	*	cp = Color + 80 + 160 * iy;
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

struct Enemy
{
	int		px;
	byte	py;
	sbyte	dx;
	byte	state, pad0, pad1, pad2;
}	enemies[5];

int	spx = 40;
sbyte	vpx = 16;
sbyte	ax = 0;
char	spy = 100;
char	fdelay = 0;
char	edelay = 5;
char	ecount = 0;

void enemies_move(void)
{
	for(char i=0; i<5; i++)
	{
		if (enemies[i].state)
		{
			enemies[i].px += enemies[i].dx;

			int	rx = enemies[i].px - spx;
			if (rx < -192 || rx >= 480)
			{
				enemies[i].state = 0;
				ecount--;
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
					spr_image(2 + i, 127 - (enemies[i].state >> 2));
					enemies[i].state--;
					if (enemies[i].state == 0)
					{
						spr_show(2 + i, false);
						ecount--;
					}
				}
			}
		}
	}
}

void enemies_spawn(void)
{
	char	u = rand();

	char	e = ecount;

	__assume(e < 5);

	sbyte	v = 1 + (u & 3);
	enemies[ecount].py = 21 + 32 * e;
	enemies[ecount].dx = vpx < 0 ? v : -v;
	enemies[ecount].px = (vpx < 0 ? spx - 56 : spx + 320) + ((u >> 1) & 31);
	enemies[ecount].state = 0x80;

	int	rx = enemies[ecount].px - spx;

	spr_set(2 + ecount, true, rx + 24, enemies[ecount].py + 50, vpx < 0 ? 97 : 96, VCOL_YELLOW, true, false, false);
	ecount++;
}

void shots_check(void)
{
	Shot	*	ps = shots;

	for(char i=0; i<5; i++)
	{
		if (enemies[i].state & 0x80)
		{
			sbyte	rx = (enemies[i].px - spx) >> 3;
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
						break;
					}						

					ps = ss;
					ss = ps->next;
				}
			}
		}
	}
}

int main(void)
{
	cia_init();

	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);

	char * dp = Font + 248 * 8;
	memset(dp, 0xff, 8);

	memcpy(Sprites, spriteset, 4096);
	mmap_set(MMAP_NO_ROM);

	tiles_unpack();

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	// initialize raster IRQ
	rirq_init(true);

	// Set scroll offset and star at top of screen
	rirq_build(&top, 2);
	rirq_write(&top, 0, &vic.ctrl2, VIC_CTRL2_MCM);
	rirq_write(&top, 1, Font + 248 * 8 + 2, 0xff);
	rirq_set(0, 58, &top);

	// Switch to text mode for status line and poll joystick at bottom
	rirq_build(&bottom, 1);
	rirq_write(&bottom, 0, &vic.ctrl2, VIC_CTRL2_MCM | VIC_CTRL2_CSEL);
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

	memset(Screen, 0, 1000);
	memset(Color, 8, 1000);

	for(int i=0; i<24; i++)
		stars[i] = rand() % 40 + 40 * (i & 3);

	shot_init();

	spr_set(0, true, 160, 100, 64,          VCOL_BLUE, true, false, false);
	spr_set(7, true, 160, 100, 64 + 16, VCOL_MED_GREY, true, false, false);
	vic.spr_priority = 0x80;

	vpx = 2;
	for(;;)
	{
		joy_poll(0);

		if (ax == 0)
			ax = joyx[0];

		spy += 2 * joyy[0];
		if (spy < 6)
			spy = 6;
		else if (spy > 6 + 159)
			spy = 6 + 159;

		if (ax > 0)
		{
			if (vpx < 32)
				vpx++;
			else
				ax = 0;

			if (vpx >= 32)
			{
				spr_image(0, 64);
				spr_image(7, 64 + 16);
			}
			else
			{
				spr_image(0, 76 + (vpx >> 3));
				spr_image(7, 76 + (vpx >> 3) + 16);
			}
		}
		else if (ax < 0)
		{
			if (vpx > - 32)
				vpx--;
			else
				ax = 0;

			if (vpx <= -32)
			{
				spr_image(0, 72);			
				spr_image(7, 72 + 16);
			}
			else
			{
				spr_image(0, 68 - (vpx >> 3));
				spr_image(7, 68 - (vpx >> 3) + 16);
			}
		}

		if (fdelay)
			fdelay--;
		else if (joyb[0] && vpx != 0)
		{
			shot_add(vpx, spy);
			fdelay = 6;
		}

		spr_move(0, 172 - 4 * vpx, 50 + spy);
		spr_move(7, 180 - 4 * vpx, 58 + spy);

		vic.color_border = VCOL_BLACK;
		vic_waitTop();
		while (vic.raster < 82)
			;

		vic.color_border = VCOL_BLUE;
		tiles_draw(spx & 4095);
		vic.color_border = VCOL_WHITE;

		if (edelay)
		{
			edelay--;
			if (edelay < 10 && !(edelay & 1))
				enemies_spawn();
		}
		else
		{
			enemies_move();
			if (!ecount)
				edelay = 64 + (rand() & 63);
		}

		shots_check();

		spx += vpx >> 2;
	}

	return 0;
}
