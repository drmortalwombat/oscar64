#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <conio.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

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

char field[160];

void fill(void)
{
	for(int i=0; i<160; i++)
		field[i] = i;
}

void shuffle(void)
{
	for(int i=0; i<160; i++)
	{
		int j = rand() % 160;
		char t = field[i];
		field[i] = field[j];
		field[j] = t;
	}	
}

void draw(unsigned i)
{
	bmu_line(&Screen, 2 * i, 0, 2 * i, field[i], 0x00, LINOP_SET);
	bmu_line(&Screen, 2 * i, field[i], 2 * i, 160, 0xff, LINOP_SET);
}

void partition(int l, int r)
{
	while (l < r)
	{
		int i = l;
		int j = r;		
		char pi = field[(r + l) >> 1];
		while (i <= j)
		{
			while (field[i] > pi)
				i++;
			while (field[j] < pi)
				j--;
			if (i <= j)
			{
				char t = field[i];
				field[i] = field[j];
				field[j] = t;
				draw(i);
				draw(j);
				i++;
				j--;
			}
		}

		partition(l, j);
		l = i;
	}
}

int main(void)
{
	init();

	fill();
	shuffle();

	for(int i=0; i<160; i++)
		draw(i);

	clock_t	t0 = clock();
	partition(0, 159);
	clock_t	t1 = clock();

	char	t[20];
	sprintf(t, "TIME : %.1f SECS.", (float)(t1 - t0) / 60);
	bmu_put_chars(&Screen, 4, 170, t, strlen(t), BLTOP_COPY);

	done();
	
	return 0;
}
