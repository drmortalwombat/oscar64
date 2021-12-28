#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <c64/rasterirq.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdio.h>
#include <c64/charwin.h>

#define Color	((char *)0xc800)
#define Hires	((char *)0xe000)

char white[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
char check[] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};
char black[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


Bitmap		Screen;

RIRQCode	rirqtop, rirqbottom;

#pragma align(rirqtop, 32)
#pragma align(rirqbottom, 32)

CharWin		twin;
CharWin		ewin;

int main(void)
{
	mmap_trampoline();

	mmap_set(MMAP_CHAR_ROM);
	memcpy((char *)0xd000, (char *)0xd000, 4096);
	mmap_set(MMAP_NO_ROM);

	rirq_init(true);

	vic.ctrl1 = VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3;
	vic.ctrl2 = VIC_CTRL2_CSEL;
	
	vic.color_back = VCOL_BLACK;

	memset(Color, 0x10, 1000);

	vic_setbank(3);
	vic.memptr = 0x28;

	rirq_build(&rirqtop, 2);    
	rirq_write(&rirqtop, 0, &vic.memptr, 0x28);
	rirq_write(&rirqtop, 1, &vic.ctrl1, VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);

	rirq_build(&rirqbottom, 3); 
	rirq_delay(&rirqbottom, 10);
	rirq_write(&rirqbottom, 1, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	rirq_write(&rirqbottom, 2, &vic.memptr, 0x26);

	rirq_set(0, 10,          &rirqtop);
	rirq_set(1, 49 + 8 * 20, &rirqbottom);
	rirq_sort();

	rirq_start();

	bm_init(&Screen, Hires, 40, 25);

	bmu_rect_pattern(&Screen, 0, 0, 320, 160, check);
	bmu_rect_fill(&Screen, 0, 159, 320, 1);
	bmu_rect_clear(&Screen, 0, 158, 320, 1);

	cwin_init(&twin, Color, 0, 20, 40, 4);
	cwin_init(&ewin, Color, 0, 24, 40, 1);

	ClipRect	rect = {0, 0, 320, 158};

	cwin_clear(&twin);
	cwin_putat_string(&twin, 0, 0, p"Enter x, y and radius for circle", 0x07);

	for(;;)
	{
		cwin_clear(&ewin);
		cwin_cursor_move(&ewin, 0, 0);

		mmap_set(MMAP_NO_BASIC);
		cwin_edit(&ewin);
		mmap_set(MMAP_RAM);

		char str[40];
		cwin_read_string(&ewin, str);

		int n, x, y, r;
		n = sscanf(str, "%d %d %d", &x, &y, &r);

		cwin_putat_string(&twin, 0, 1, str, 0x0e);
		sprintf(str, p"N: %D X: %3D Y: %3D R: %2D", n, x, y, r);
		cwin_putat_string(&twin, 0, 2, str, 0x07);

		if (n == 3)
		{
			bm_circle_fill(&Screen, &rect, x, y, r + 1, black);
			bm_circle_fill(&Screen, &rect, x, y, r - 1, white);
		}

	}

	return 0;
}
