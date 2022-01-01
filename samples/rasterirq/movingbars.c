#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <string.h>
#include <math.h>
#include <conio.h>

// Five raster IRQs for top and bottom of the two chasing bars, and the bottom
// of the screen
RIRQCode	ftop, fbottom, btop, bbottom, final ;

char sintab[256];

int main(void)
{
	rirq_init(true);

	rirq_build(&ftop, 3);
	rirq_delay(&ftop, 10);
	rirq_write(&ftop, 1, &vic.color_back, 2);
	rirq_write(&ftop, 2, &vic.color_border, 2);

	rirq_build(&fbottom, 3);
	rirq_delay(&fbottom, 10);
	rirq_write(&fbottom, 1, &vic.color_back, 6);
	rirq_write(&fbottom, 2, &vic.color_border, 14);

	rirq_build(&btop, 3);
	rirq_delay(&btop, 10);
	rirq_write(&btop, 1, &vic.color_back, 7);
	rirq_write(&btop, 2, &vic.color_border, 7);

	rirq_build(&bbottom, 3);
	rirq_delay(&bbottom, 10);
	rirq_write(&bbottom, 1, &vic.color_back, 6);
	rirq_write(&bbottom, 2, &vic.color_border, 14);

	rirq_build(&final, 0);

	char	yfront = 100, yback = 200;

	rirq_set(0, yfront, &ftop);
	rirq_set(1, yfront + 16, &fbottom);
	rirq_set(2, yback, &btop);
	rirq_set(3, yback + 16, &bbottom);
	rirq_set(4, 250, &final);
	rirq_sort();


	rirq_start();

	for(int i=0; i<32; i++)
		sintab[i] = (int)(120 + 60 * sin(i * PI / 16)) | 1;

	char fi = 3, bi = 0;
	for(;;)
	{
		yfront = sintab[fi & 31];
		yback = sintab[bi & 31];

		rirq_move(0, yfront);
		if (yback == yfront)
		{
			rirq_move(1, yfront + 16);
			rirq_clear(2);
			rirq_clear(3);
		}
		else
		{		
			if (yback < yfront || yback > yfront + 16)
			{
				rirq_move(1, yfront + 16);
				rirq_move(2, yback);
			}
			else
			{
				rirq_clear(1);
				rirq_move(2, yfront + 16);
			}
			if (yback < yfront - 16 || yback > yfront)
				rirq_move(3, yback + 16);
			else
				rirq_clear(3);
		}

		rirq_sort();
		rirq_wait();

		fi ++;
		bi ++;
	}

	return 0;
}

