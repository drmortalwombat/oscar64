#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <string.h>

const char  Text[] =
	S"LABORUM RERUM QUO. QUASI IN, SEQUI, TENETUR VOLUPTATEM RERUM "
	S"PORRO NON ET MAIORES ALIAS ODIO EST EOS. MAGNAM APERIAM CUM ET "
	S"ESSE TEMPORE ITAQUE TEMPORA VOLUPTAS ET IPSAM IPSAM EARUM. ID "
	S"SUSCIPIT QUIA RERUM REPREHENDERIT ERROR ET UT. DOLOR ID "
	S"CORPORIS, EOS? UNDE VERO ISTE QUIA? EAQUE EAQUE. IN. AUT ID "
	S"EXPEDITA ILLUM MOLESTIAS, ";

// Raster interrupt command structure for change to scrolled and back

RIRQCode	scroll, restore;

static int x;

// Loop through text
__interrupt void doscroll(void)
{
	// Update raster IRQ for scroll line with new horizontal scroll offset		
	rirq_data(&scroll, 0, 7 - (x & 7));
	// Copy scrolled version of text when switching over char border
	if ((x & 7) == 0)
		memcpy((char *)0x0400 + 40 * 24, Text + ((x >> 3) & 255), 40);
	x++;
}

int main(void)
{
	// initialize raster IRQ
	rirq_init(true);

	// Build switch to scroll line IRQ
	rirq_build(&scroll, 1);
	// Change control register two with this IRQ
	rirq_write(&scroll, 0, &vic.ctrl2, 0);
	// Put it onto the scroll line
	rirq_set(0, 50 + 24 * 8, &scroll);

	// Build the switch to normal IRQ
	rirq_build(&restore, 2);
	// re-enable left and right column and reset horizontal scroll
	rirq_write(&restore, 0, &vic.ctrl2, VIC_CTRL2_CSEL);
	// call scroll copy code
	rirq_call(&restore, 1, doscroll);
	// place this at the top of the screen before the display starts
	rirq_set(1, 4, &restore);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	return 0;
}
