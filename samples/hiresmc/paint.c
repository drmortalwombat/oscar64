#include <c64/vic.h>
#include <c64/memmap.h>
#include <gfx/mcbitmap.h>
#include <c64/mouse.h>
#include <c64/joystick.h>
#include <c64/keyboard.h>
#include <c64/cia.h>
#include <c64/sprites.h>
#include <string.h>
#include <oscar.h>

#pragma region(main, 0x0880, 0xd000, , , {code, data, bss, heap, stack} )

static char * const Color1	=	(char *)0xd000;
static char * const Color2	=	(char *)0xd800;
static char * const Hires	=	(char *)0xe000;
static char * const Sprites	=	(char *)0xd800;

const char MouseSpriteData[] = {
	#embed spd_sprites lzo "../resources/mouse.spd"
};

Bitmap		sbm;
ClipRect	scr = { 0, 0, 320, 200 };

void init(void)
{
	// Install IRQ trampoline
	mmap_trampoline();

	// All RAM
	mmap_set(MMAP_RAM);	

	// Init hires mem, and resources
	memset(Color1, 0x67, 1000);
	memset(Hires, 0, 8000);
	oscar_expand_lzo(Sprites, MouseSpriteData);

	// Sprite image for cursor
	Color1[0x3f8] = 97;
	Color1[0x3f9] = 96;

	// Enable IO space
	mmap_set(MMAP_NO_ROM);

	// Clear color RAM
	memset(Color2, 0x02, 1000);

	// Set screen
	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;
	vic_setmode(VICM_HIRES_MC, Color1, Hires);

	// Init mouse cursor
	spr_show(0, true);
	spr_show(1, true);
	spr_color(0, VCOL_BLACK);
	spr_color(1, VCOL_WHITE);
	spr_move(0, 24, 50);
	spr_move(1, 24, 50);

	// Disable system interrupt and init mouse
	cia_init();
	mouse_init();

	bm_init(&sbm, Hires, 40, 25);
}

int mouse_x, mouse_y;

bool mouse_move(void)
{
	// Poll mouse and joystick for backup
	joy_poll(0);
	mouse_poll();

	// New mouse cursor position
	int mx = mouse_x + (signed char)(joyx[0] + mouse_dx);
	int my = mouse_y + (signed char)(joyy[0] - mouse_dy);

	// Clip to screen
	if (mx < 0)
		mx = 0;
	else if (mx > 319)
		mx = 319;
	if (my < 0)
		my = 0;
	else if (my > 199)
		my = 199;

	// Check if moved
	if (mx != mouse_x || my != mouse_y)
	{
		mouse_x = mx;
		mouse_y = my;

		// Update cursor sprite
		spr_move(0, mx + 24, my + 50);
		spr_move(1, mx + 24, my + 50);

		return true;
	}

	return false;
}

int main(void)
{
	init();

	char c0 = 1, c1 = 1;

	for(;;)
	{
		// Check if mouse moved
		if (mouse_move())
		{
			// Paint a circle at the mouse position, if mouse was moved

			if (mouse_lb || joyb[0])
				bmmc_circle_fill(&sbm, &scr, mouse_x, mouse_y, 5, MixedColors[c0][c1]);
			else if (mouse_rb)
				bmmc_circle_fill(&sbm, &scr, mouse_x, mouse_y, 5, MixedColors[0][0]);
		}

		// Poll the keyboard
		keyb_poll();

		switch (keyb_key)
		{
		// Clear screen
		case KSCAN_HOME + KSCAN_QUAL_DOWN:
			bmmcu_rect_fill(&sbm, 0, 0, 320, 200, 0);
			break;

		// Select color with 0..9
		case KSCAN_0 + KSCAN_QUAL_DOWN:
			c0 = 0; c1 = 0;
			break;
		case KSCAN_1 + KSCAN_QUAL_DOWN:
			c0 = 1; c1 = 1;
			break;
		case KSCAN_2 + KSCAN_QUAL_DOWN:
			c0 = 2; c1 = 2;
			break;
		case KSCAN_3 + KSCAN_QUAL_DOWN:
			c0 = 3; c1 = 3;
			break;

		case KSCAN_4 + KSCAN_QUAL_DOWN:
			c0 = 1; c1 = 0;
			break;
		case KSCAN_5 + KSCAN_QUAL_DOWN:
			c0 = 1; c1 = 2;
			break;
		case KSCAN_6 + KSCAN_QUAL_DOWN:
			c0 = 1; c1 = 3;
			break;
		case KSCAN_7 + KSCAN_QUAL_DOWN:
			c0 = 2; c1 = 0;
			break;
		case KSCAN_8 + KSCAN_QUAL_DOWN:
			c0 = 2; c1 = 3;
			break;
		case KSCAN_9 + KSCAN_QUAL_DOWN:
			c0 = 3; c1 = 0;
			break;

		// Flood fill
		case KSCAN_F + KSCAN_QUAL_DOWN:
			bmmc_flood_fill(&sbm, &scr, mouse_x, mouse_y, c0);
			break;
		}

		// Wait one frame
		vic_waitFrame();
	}

	return 0;
}
