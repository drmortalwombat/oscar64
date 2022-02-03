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
	Screen[40 * y + x +  0] =  96; Color[40 * y + x +  0] = c;
	Screen[40 * y + x +  1] =  97; Color[40 * y + x +  1] = c;
	Screen[40 * y + x +  2] =  98; Color[40 * y + x +  2] = c;
	Screen[40 * y + x + 40] =  99; Color[40 * y + x + 40] = c;
	Screen[40 * y + x + 41] = 100; Color[40 * y + x + 41] = c;
	Screen[40 * y + x + 42] = 101; Color[40 * y + x + 42] = c;
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

	if (c >= 96 && c < 102)
	{
		c -= 96;

		if (c >= 3)
		{
			y --;
			c -= 3;
		}

		x -= c;

		Screen[40 * y + x +  0] = 32;
		Screen[40 * y + x +  1] = 32;
		Screen[40 * y + x +  2] = 32;
		Screen[40 * y + x + 40] = 32;
		Screen[40 * y + x + 41] = 32;
		Screen[40 * y + x + 42] = 32;
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
	vic.color_back = VCOL_BLUE;
	vic.color_back1 = VCOL_WHITE;
	vic.color_back2 = VCOL_DARK_GREY;

	memset(Screen, ' ', 1000);

	brick_init();

	float	sx = 100, sy = 180, vx = 3.2, vy = -4.0;

	spr_set(0, true, 200, 200, 96, 1, false, false, false);

	for(;;)		
	{
		vic.color_border++;

		for(char i=0; i<4; i++)
		{
			sx += vx;
			sy += vy;
	//		vy += 0.01;

			bool	mirrorX = false, mirrorY = false;

			if (sx + 6 > 320 || sx < 0)
				mirrorX = true;
			if (sy + 6 > 200 || sy < 0)
				mirrorY = true;

			int	x0 = (int)sx >> 3;
			int y0 = (int)sy >> 3;
			int	x1 = (int)(sx + 6) >> 3;
			int y1 = (int)(sy + 6) >> 3;

			bool c00 = y0 >= 0 && Screen[40 * y0 + x0] >= 96;
			bool c01 = y0 >= 0 && Screen[40 * y0 + x1] >= 96;
			bool c10 = y1 < 24 && Screen[40 * y1 + x0] >= 96;
			bool c11 = y1 < 24 && Screen[40 * y1 + x1] >= 96;

			if (vx < 0 && (c00 && !c01 || c10 && !c11))
				mirrorX = true;
			else if (vx > 0 && (c01 && !c00 || c11 && !c10))
				mirrorX = true;

			if (vy < 0 && (c00 && !c10 || c01 && !c11))
				mirrorY = true;
			else if (vy > 0 && (c10 && !c00 || c11 && !c01))
				mirrorY = true;

			if (c00 || c01 || c10 || c11)
			{
				brick_clr(x0, y0);
				brick_clr(x1, y0);
				brick_clr(x0, y1);
				brick_clr(x1, y1);
			}

			if (mirrorY)
			{
				vy = -vy;
				sy += 2 * vy;
	//			vy *= 0.99;
			}
			if (mirrorX)
			{
				vx = -vx;
				sx += 2 * vx;
	//			vx *= 0.99;			
			}
		}

		spr_move(0, sx + 24, sy + 50);

		vic.color_border--;

		vic_waitFrame();
	}	

	return 0;
}
