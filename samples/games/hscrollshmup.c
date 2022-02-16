#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <string.h>
#include <stdlib.h>

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xd000;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd800;

// Character set 
char charset[2048] = {
	#embed "../../../assets/uridium1 - Chars.bin"
};

char tileset[] = {
	#embed "../../../assets/uridium1 - Tiles.bin"	
};

char tilemap[64 * 5] = {
	#embed "../../../assets/uridium1 - Map (64x5).bin"		
};

char spriteset[2048] = {
	#embed 2048 0 "../../../assets/uridium1 - Sprites.bin"
};

char xtileset[16][20];
char stars[24];

void tiles_unpack(void)
{
	for(char t=0; t<20; t++)
	{
		for(char i=0; i<16; i++)
			xtileset[i][t] = tileset[16 * t + i];
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

void tiles_draw(unsigned x)
{
	char	xs = 7 - (x & 7);

	vic.ctrl2 = VIC_CTRL2_MCM + xs;

	x >>= 3;

	char	xl = x >> 2, xr = x & 3;
	char	yl = 0;

	for(int iy=0; iy<5; iy++)
	{
		char	*	dp = Screen + 80 + 160 * iy;
		char	*	cp = Color + 80 + 160 * iy;
		char	*	tp = tilemap + xl + 64 * iy;

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

		xs |= 248;

		char	k = stars[yl + 0] +   0;
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = xs;			
		}

		k = stars[yl + 1];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = xs;			
		}

		k = stars[yl + 2];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] = xs;			
		}

		k = stars[yl + 3];
		if (dp[k])
			cp[k] = 8;
		else
		{
			cp[k] = 0;
			dp[k] =  xs;			
		}

		yl += 4;
	}
}

int main(void)
{
	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);

	char * dp = Font + 248 * 8;

	for(int i=0; i<8; i++)
	{
		for(int j=0; j<8; j++)
		{
			if (j == 2)
				dp[8 * i + j] = ~(1 << i);
			else
				dp[8 * i + j] = 0xff;
		}
	}

	memcpy(Sprites, spriteset, 2048);
	mmap_set(MMAP_NO_BASIC);

	tiles_unpack();

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	// Change colors
	vic.color_border = VCOL_BLUE;
	vic.color_back = VCOL_WHITE;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_DARK_GREY;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	memset(Screen, 0, 1000);
	memset(Color, 8, 1000);

	for(int i=0; i<24; i++)
		stars[i] = rand() % 40 + 40 * (i & 3);

	spr_set(0, true, 160, 100, 96, 6, true, false, false);

	int	spx = 40;
	int	vpx = 16;
	int	ax = 0;
	char	spy = 100;

	for(;;)
	{
		joy_poll(0);

		if (ax == 0)
			ax = joyx[0];

		spy += 2 * joyy[0];

		if (ax > 0)
		{
			if (vpx < 16)
				vpx++;
			if (vpx == 16)
			{
				spr_image(0, 96);
				ax = 0;
			}
			else
				spr_image(0, 108 + (vpx >> 2));
		}
		else if (ax < 0)
		{
			if (vpx > -15)
				vpx--;
			if (vpx == -15)
			{
				spr_image(0, 104);			
				ax = 0;
			}
			else
				spr_image(0, 100 - (vpx >> 2));
		}

		spr_move(0, 160 - 4 * vpx, 50 + spy);

		vic_waitFrame();
		vic.color_border++;
		tiles_draw(spx);
		vic.color_border--;
		spx += vpx >> 1;
	}

	return 0;
}
