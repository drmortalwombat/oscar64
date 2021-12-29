#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <conio.h>


#define Color	((char *)0xd000)
#define Hires	((char *)0xe000)

Bitmap		Screen;

void init(void)
{
	mmap_trampoline();
	mmap_set(MMAP_RAM);

	memset(Color, 0x01, 1000);
	memset(Hires, 0x00, 8000);

	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES, Color, Hires);

	vic.color_border = VCOL_WHITE;

	bm_init(&Screen, Hires, 40, 25);	
}

void done(void)
{
	mmap_set(MMAP_ROM);

	getch();

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
}

void draw(ClipRect * cr, byte pattern)
{
	for(int i=0; i<40; i ++)
	{
		bm_line(&Screen, cr, 8 * i, 0, 319, 5 * i, pattern);
		bm_line(&Screen, cr, 319, 5 * i, 319 - 8 * i, 199, pattern);
		bm_line(&Screen, cr, 319 - 8 * i, 199, 0, 199 - 5 * i, pattern);
		bm_line(&Screen, cr, 0, 199 - 5 * i, 8 * i, 0, pattern);
	}
}

int main(void)
{
	init();

	ClipRect	cr = {0, 0, 320, 200};

	draw(&cr, 0xff);
	draw(&cr, 0x00);

	draw(&cr, 0xff);
	draw(&cr, 0xaa);
	draw(&cr, 0x88);
	draw(&cr, 0x80);
	draw(&cr, 0x00);

	done();
	
	return 0;
}
