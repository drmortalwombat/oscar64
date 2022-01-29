#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <conio.h>
#include <math.h>

#define Color	((char *)0xd000)
#define Hires	((char *)0xe000)

Bitmap		Screen;

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

int main(void)
{
	init();

	bmu_rect_pattern(&Screen, 0, 0, 320, 200, NineShadesOfGrey[4]);
	bmu_rect_clear(&Screen, 14, 14, 304, 184);
	bmu_rect_fill(&Screen, 8, 8, 304, 184);
	bmu_rect_clear(&Screen, 10, 10, 300, 180);

	ClipRect	cr = {11, 11, 309, 189};

	float	px[10], py[10];

	for(int i=0; i<10; i++)
	{
		float	w = i * PI / 5, c = cos(w), s = sin(w), r = (i & 1) ? 1.0 : 0.4;
		px[i] = r * c; py[i] = r * s;

	}

	for(int i=0; i<128; i++)
	{
		int rpx[10], rpy[10];
		float	r = i + 4;
		float	w = i * PI / 16, c = r * cos(w), s = r * sin(w), cw = r * cos(w * 2.0), sw = r * sin(w * 2.0);

		for(int j=0; j<10; j++)
		{			
			float	fx = px[j], fy = py[j];
			rpx[j] = 160 + cw + fx * c + fy * s;
			rpy[j] = 100 + sw - fx * s + fy * c;
		}

		bm_polygon_nc_fill(&Screen, &cr, rpx, rpy, 10, NineShadesOfGrey[i % 9]);

		for(int j=0; j<10; j++)
		{
			int k = (j + 1) % 10;
			bm_line(&Screen, &cr, rpx[j], rpy[j], rpx[k], rpy[k], 0xff, LINOP_SET);
		}
	}

	done();

	return 0;
}
