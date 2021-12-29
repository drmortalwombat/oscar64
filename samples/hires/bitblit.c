#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <conio.h>
#include <math.h>

#define Color	((char *)0xd000)
#define Hires	((char *)0xe000)

Bitmap		Screen, Brush;

void init(void)
{
	mmap_trampoline();
	mmap_set(MMAP_RAM);

	memset(Color, 0x10, 1000);
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

int poly_x[] = {0, 32, 32, 24, 40, 32, 32, 64, 52, 42, 22, 12};
int poly_y[] = {64, 0, 20, 36, 36, 20, 0, 64, 64, 44, 44, 64};

struct BlitDemo
{
	const char	*	name;
	BlitOp			op;
};

BlitDemo	blitDemos[11] = {

	{"SET",		BLTOP_SET},
	{"RESET",	BLTOP_RESET},
	{"NOT",		BLTOP_NOT},

	{"XOR",		BLTOP_XOR},
	{"OR",		BLTOP_OR},
	{"AND",		BLTOP_AND},
	{"NAND",	BLTOP_AND_NOT},

	{"COPY",	BLTOP_COPY},
	{"NCOPY",	BLTOP_NCOPY},

	{"PATTERN",	BLTOP_PATTERN},
	{"MASKPAT",	BLTOP_PATTERN_AND_SRC},
};

char pat[] = {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00};

int main(void)
{
	init();

	bm_alloc(&Brush, 8, 8);
	bm_fill(&Brush, 0);

	ClipRect	bcr = {0, 0, 64, 64};

	bm_polygon_nc_fill(&Brush, &bcr, poly_x, poly_y, 12, NineShadesOfGrey[8]);

	bmu_rect_pattern(&Screen, 0, 0, 320, 200, NineShadesOfGrey[2]);

	ClipRect	scr = {0, 0, 320, 200};

	for(int i=0; i<11; i++)
	{
		int	dx = 80 * ((i + 1) % 4);
		int dy = 66 * ((i + 1) / 4);

		bmu_bitblit(&Screen, dx + 8, dy + 1, &Brush, 0, 0, 64, 64, pat, blitDemos[i].op);
		bm_put_string(&Screen, &scr, dx + 8, dy + 20, blitDemos[i].name, BLTOP_COPY);
	}

	done();

	return 0;
}
