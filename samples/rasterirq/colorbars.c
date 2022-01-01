#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <conio.h>

// Prepare small 14 color bars and one IRQ for back to normal
RIRQCode	bars[15];

int main(void)
{
	// initialize raster IRQ
	rirq_init(true);

	for(int i=0; i<15; i++)
	{
		// Build color change raster IRQ
		rirq_build(bars + i, 2);
		// Change border color
		rirq_write(bars + i, 0, &vic.color_border, i);
		// Change background color
		rirq_write(bars + i, 1, &vic.color_back, i);
		// Place it on screen
		rirq_set(i, 80 + 8 * i, bars + i);
	}

	// Sort all raster IRQs
	rirq_sort();

	// Start raster IRQs
	rirq_start();

	return 0;
}
