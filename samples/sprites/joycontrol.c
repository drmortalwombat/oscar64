#include <c64/sprites.h>
#include <c64/joystick.h>
#include <string.h>

char spdata[] = {
	#embed "../resources/friendlybear.bin"	
};

int	spx, spy;

#define SpriteData	((char *)0x0380)

int main(void)
{
	// Copy the sprite data
	memcpy(SpriteData, spdata, 128);

	// Initalize the sprite system
	spr_init((char*)0x0400);

	// Center screen position
	spx = 160;
	spy = 100;

	// Two sprites, one black in front hires and one multicolor in back
	spr_set(0, true, spx, spy, 0x03c0 / 64, VCOL_BLACK, false, false, false);
	spr_set(1, true, spx, spy, 0x0380 / 64, VCOL_ORANGE, true, false, false);

	// Multicolor sprite colors
  	vic.spr_mcolor0 = VCOL_BROWN;
    vic.spr_mcolor1 = VCOL_WHITE;

	for(;;)
	{
		// Poll joytick
		joy_poll(1);

		// Change position according to joystick
		spx += joyx[1];
		spy += joyy[1];

		// Move sprites on screen
		spr_move(0, spx, spy);
		spr_move(1, spx, spy);

		// Wait for one frame iteration
		vic_waitFrame();
	}

	return 0;
}

