#include <c64/vic.h>
#include <c64/memmap.h>
#include <gfx/mcbitmap.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <math.h>

#pragma region(main, 0x0a00, 0xc800, , , {code, data, bss, heap, stack} )

#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)
#define Hires	((char *)0xe000)

Bitmap		sbm;

int main(void)
{
	mmap_trampoline();

	vic_setmode(VICM_HIRES_MC, Color1, Hires);

	mmap_set(MMAP_NO_ROM);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	memset(Color1, 0x67, 1000);
	memset(Color2, 0x02, 1000);
	memset(Hires, 0, 8000);

	bm_init(&sbm, Hires, 40, 25);

	ClipRect	scr = { 0, 0, 320, 200 };

	for(;;)
	{
		for(int i=0; i<20; i++)
		{
			bmmc_circle_fill(&sbm, &scr, rand() % 320, rand() % 200, 5 + rand() % 40, MixedColors[1][1]);
		}
		for(int i=0; i<20; i++)
		{
			bmmc_circle_fill(&sbm, &scr, rand() % 320, rand() % 200, 5 + rand() % 40, MixedColors[0][0]);
		}
		bmmc_flood_fill(&sbm, &scr, 210, 100, 2);
		bmmc_flood_fill(&sbm, &scr, 60, 140, 3);
	}

	mmap_set(MMAP_ROM);

	getch();

	return 0;
}
