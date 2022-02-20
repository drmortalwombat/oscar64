#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <c64/cia.h>
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

char spriteset[2048] = {
	#embed 2048 0 "../../../assets/uridium1 - Sprites.bin"
};

char xtileset[16][64];
char xtilemap[144 * 5];
char stars[24];

#pragma align(xtileset, 64);

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
}

void tiles_draw0(char * dp, char * tm)
{
	for(char x=0; x<10; x++)
	{
		char	ti = tm[x];

		dp[  0] = xtileset[ 0][ti];
		dp[  1] = xtileset[ 1][ti];
		dp[  2] = xtileset[ 2][ti];
		dp[  3] = xtileset[ 3][ti];
		dp[ 40] = xtileset[ 4][ti];
		dp[ 41] = xtileset[ 5][ti];
		dp[ 42] = xtileset[ 6][ti];
		dp[ 43] = xtileset[ 7][ti];
		dp[ 80] = xtileset[ 8][ti];
		dp[ 81] = xtileset[ 9][ti];
		dp[ 82] = xtileset[10][ti];
		dp[ 83] = xtileset[11][ti];
		dp[120] = xtileset[12][ti];
		dp[121] = xtileset[13][ti];
		dp[122] = xtileset[14][ti];
		dp[123] = xtileset[15][ti];

		dp += 4;
	}
}

void tiles_draw3(char * dp, char * tm)
{
	char	ti = tm[0];

	for(char x=1; x<11; x++)
	{
		dp[  0] = xtileset[ 3][ti];
		dp[ 40] = xtileset[ 7][ti];
		dp[ 80] = xtileset[11][ti];
		dp[120] = xtileset[15][ti];

		ti = tm[x];

		dp[  1] = xtileset[ 0][ti];
		dp[  2] = xtileset[ 1][ti];
		dp[  3] = xtileset[ 2][ti];
		dp[ 41] = xtileset[ 4][ti];
		dp[ 42] = xtileset[ 5][ti];
		dp[ 43] = xtileset[ 6][ti];
		dp[ 81] = xtileset[ 8][ti];
		dp[ 82] = xtileset[ 9][ti];
		dp[ 83] = xtileset[10][ti];
		dp[121] = xtileset[12][ti];
		dp[122] = xtileset[13][ti];
		dp[123] = xtileset[14][ti];
		
		dp += 4;
	}
}

void tiles_draw2(char * dp, char * tm)
{
	char	ti = tm[0];
	
	for(char x=1; x<11; x++)
	{
		dp[  0] = xtileset[ 2][ti];
		dp[  1] = xtileset[ 3][ti];
		dp[ 40] = xtileset[ 6][ti];
		dp[ 41] = xtileset[ 7][ti];
		dp[ 80] = xtileset[10][ti];
		dp[ 81] = xtileset[11][ti];
		dp[120] = xtileset[14][ti];
		dp[121] = xtileset[15][ti];

		ti = tm[x];

		dp[  2] = xtileset[ 0][ti];
		dp[  3] = xtileset[ 1][ti];
		dp[ 42] = xtileset[ 4][ti];
		dp[ 43] = xtileset[ 5][ti];
		dp[ 82] = xtileset[ 8][ti];
		dp[ 83] = xtileset[ 9][ti];
		dp[122] = xtileset[12][ti];
		dp[123] = xtileset[13][ti];
		
		dp += 4;
	}
}

void tiles_draw1(char * dp, char * tm)
{
	char	ti = tm[0];

	for(char x=1; x<11; x++)
	{
		dp[  0] = xtileset[ 1][ti];
		dp[  1] = xtileset[ 2][ti];
		dp[  2] = xtileset[ 3][ti];
		dp[ 40] = xtileset[ 5][ti];
		dp[ 41] = xtileset[ 6][ti];
		dp[ 42] = xtileset[ 7][ti];
		dp[ 80] = xtileset[ 9][ti];
		dp[ 81] = xtileset[10][ti];
		dp[ 82] = xtileset[11][ti];
		dp[120] = xtileset[13][ti];
		dp[121] = xtileset[14][ti];
		dp[122] = xtileset[15][ti];

		ti = tm[x];

		dp[  3] = xtileset[ 0][ti];
		dp[ 43] = xtileset[ 4][ti];
		dp[ 83] = xtileset[ 8][ti];
		dp[123] = xtileset[12][ti];
		
		dp += 4;
	}
}


struct Shot
{
	char	x, y, dx, n;
}	shots[5][4];

inline void shot_draw(char * dp, char i, char xp, char yp)
{
	__assume(i < 32);
	__assume(yp < 8);

	char		c = dp[xp];
	dp[xp] = i | 0xe0;

	char	*	fsp = Font + 8 * c;
	char	*	fdp = (Font + 0xe0 * 8) + 8 * i;

	fdp[0] = fsp[0]; fdp[1] = fsp[1]; fdp[2] = fsp[2]; fdp[3] = fsp[3];
	fdp[4] = fsp[4]; fdp[5] = fsp[5]; fdp[6] = fsp[6]; fdp[7] = fsp[7];

	fdp[yp] = 0x00;

}

void tiles_draw(unsigned x)
{
	char	xs = 7 - (x & 7);

	x >>= 3;

	char	xl = x >> 2, xr = x & 3;
	char	yl = 0;
	char	ci = 0;

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


		Shot	*	s = shots[iy];
		for(char si=0; si<4; si++)
		{
			if (s->n)
			{	
				s->x += s->dx;
				s->n--;
				shot_draw(dp, ci++, s->x, s->y);
			}
			s++;
		}

		yl += 4;
	}

	Font[248 * 8 + 2] = ~(1 << xs);

	vic.ctrl2 = VIC_CTRL2_MCM + xs;	
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

	for(int i=0; i<5; i++)
	{
		for(int j=0; j<8; j++)
		{
			shots[i][j].x = rand() % 160;
			shots[i][j].y = rand() & 7;
		}
	}

	spr_set(0, true, 160, 100, 64,          VCOL_BLUE, true, false, false);
	spr_set(1, true, 160, 100, 64 + 16, VCOL_MED_GREY, true, false, false);
	vic.spr_priority = 2;

	int	spx = 40;
	int	vpx = 16;
	int	ax = 0;
	char	spy = 100;
	char	fdelay = 0;

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
				spr_image(1, 64 + 16);
			}
			else
			{
				spr_image(0, 76 + (vpx >> 3));
				spr_image(1, 76 + (vpx >> 3) + 16);
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
				spr_image(1, 72 + 16);
			}
			else
			{
				spr_image(0, 68 - (vpx >> 3));
				spr_image(1, 68 - (vpx >> 3) + 16);
			}
		}

		if (fdelay)
			fdelay--;
		else if (joyb[0] && vpx != 0)
		{
			char	py = spy - 6;
			char	gy = py >> 5;
			char	ey = (py >> 3) & 3;
			char	ry = py & 7;

			Shot	*	s = shots[gy];

			char	i = 0;
			while (i < 4 && s[i].n != 0)
				i++;

			if (i < 4)
			{
				s[i].y = ry;
				if (vpx < 0)
				{
					s[i].dx = -1;
					char	x = (148 - 4 * vpx) >> 3;
					s[i].n = x - 1;
					s[i].x = 40 * ey + x;
				}
				else if (vpx > 0)
				{
					s[i].dx = 1;
					char	x = (156 - 4 * vpx) >> 3;
					s[i].x = 40 * ey + x;
					s[i].n = 39 - x;
				}
				fdelay = 4;
			}
		}

		spr_move(0, 172 - 4 * vpx, 50 + spy);
		spr_move(1, 180 - 4 * vpx, 58 + spy);

		vic.color_border++;
		vic_waitLine(82);
		vic.color_border++;
		tiles_draw(spx);
		vic.color_border--;
		vic.color_border--;
		spx += vpx >> 1;

		spx &= 4095;
	}

	return 0;
}