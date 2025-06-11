#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <string.h>

const char * Text =
	S"LABORUM RERUM QUO. QUASI IN, SEQUI, TENETUR VOLUPTATEM RERUM "
	S"PORRO NON ET MAIORES ALIAS ODIO EST EOS. MAGNAM APERIAM CUM ET "
	S"ESSE TEMPORE ITAQUE TEMPORA VOLUPTAS ET IPSAM IPSAM EARUM. ID "
	S"SUSCIPIT QUIA RERUM REPREHENDERIT ERROR ET UT. DOLOR ID "
	S"CORPORIS, EOS? UNDE VERO ISTE QUIA? EAQUE EAQUE. IN. AUT ID "
	S"EXPEDITA ILLUM MOLESTIAS, ";

// Raster interrupt command structure for change to scrolled and back

RIRQCode	scroll, bottom;

int main(void)
{
	// initialize raster IRQ
	rirq_init(true);

	// Build switch to scroll line IRQ
	rirq_build(&scroll, 2);
	// Delay for one line to get to right border
	rirq_delay(&scroll, 11);
	// Change control register two with this IRQ
	rirq_write(&scroll, 1, &vic.ctrl2, 0);
	// Put it onto the scroll line
	rirq_set(0, 49 + 24 * 8, &scroll);

	// Build the switch to normal IRQ
	rirq_build(&bottom, 1);
	// re-enable left and right column and reset horizontal scroll
	rirq_write(&bottom, 0, &vic.ctrl2, VIC_CTRL2_CSEL);
	// place this at the bottom
	rirq_set(1, 250, &bottom);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// Loop through text
	int	x = 0;
	for(;;)
	{
		// wait for raster reaching bottom of screen
		rirq_wait();
		// Update raster IRQ for scroll line with new horizontal scroll offset		
		rirq_data(&scroll, 1, 7 - (x & 7));
		// Copy scrolled version of text when switching over char border
		if ((x & 7) == 0)
			memcpy((char *)0x0400 + 40 * 24, Text + ((x >> 3) & 255), 40);
		x++;
	}

	return 0;
}
