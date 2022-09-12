#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <math.h>
#include <string.h>
#include <conio.h>


char *	const	Color = (char *)0xd000;
char *	const	Hires =	(char *)0xe000;

Bitmap		Screen;
ClipRect	Clip = {0, 0, 320, 200};

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

void draw(float x, float y, float a, float s)
{
	if (s < 6.0)
		return;

	float	tx = x + cos(a) * s;
	float	ty = y + sin(a) * s;

	bm_line(&Screen, &Clip, x, y, tx, ty, 0xff, LINOP_SET);

	draw(tx, ty, a + 0.3, s * 0.9);
	draw(tx, ty, a - 0.4, s * 0.8);
}

int main(void)
{
	init();

	draw(140, 199, PI * 1.5, 32);

	done();
	
	return 0;
}
