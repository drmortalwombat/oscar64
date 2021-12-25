#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <conio.h>


RIRQCode	bars[15];

int main(void)
{
	rirq_init(true);

	for(int i=0; i<15; i++)
	{
		rirq_build(bars + i, 2);
		rirq_write(bars + i, 0, &vic.color_border, i);
		rirq_write(bars + i, 1, &vic.color_back, i);
		rirq_set(i, 80 + 8 * i, bars + i);
	}

	rirq_sort();

	rirq_start();


	return 0;
}
