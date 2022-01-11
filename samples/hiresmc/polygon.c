#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/mcbitmap.h>
#include <string.h>
#include <conio.h>
#include <math.h>

#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)
#define Hires	((char *)0xe000)

Bitmap		Screen;

void init(void)
{
	mmap_trampoline();
	mmap_set(MMAP_RAM);

	memset(Color1, 0x67, 1000);
	memset(Color2, 0x02, 1000);
	memset(Hires, 0x00, 8000);

	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES_MC, Color1, Hires);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

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

	bmmcu_rect_fill(&Screen, 0, 0, 320, 200, 1);
	bmmcu_rect_fill(&Screen, 8, 8, 304, 184, 2);
	bmmcu_rect_fill(&Screen, 10, 10, 300, 180, 0);

	ClipRect	cr = {10, 10, 310, 190};

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

		bmmc_polygon_nc_fill(&Screen, &cr, rpx, rpy, 10, MixedColors[i & 3][(i >> 2) & 3]);

		for(int j=0; j<10; j++)
		{
			int k = (j + 1) % 10;
			bmmc_line(&Screen, &cr, rpx[j], rpy[j], rpx[k], rpy[k], 0);
		}
	}

	done();

	return 0;
}
