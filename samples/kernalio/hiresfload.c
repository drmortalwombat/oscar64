#include <c64/kernalio.h>
#include <c64/memmap.h>
#include <c64/vic.h>
#include <c64/flossiec.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <stdio.h>

char * const Hires	=	(char *)0xe000;
char * const Color	=	(char *)0xd800;
char * const Screen	=	(char *)0xcc00;

int main(void)
{
	// Prepare fast loader in floppy memory
	flosskio_init(8);

	// Map filenames to track/sector addresses
	floss_blk	blks[1];
	flosskio_mapdir(p"blumba2", blks);

	// Clear screen
	memset(Hires,  0x00, 8000);
	memset(Screen, 0xff, 1000);
	memset(Color,  0x01, 1000);

	// Switch to multicolor hires
	vic_setmode(VICM_HIRES_MC, Screen, Hires);
	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	// Open file
	flosskio_open(blks[0].track, blks[0].sector);

	// Read compressed image
	flossiec_read_lzo(Hires,  8000);
	flossiec_read_lzo(Screen, 1000);
	flossiec_read_lzo(Color,  1000);

	// Close file
	flosskio_close();

	// Remove drive code
	flosskio_shutdown();

	// Wait for a character
	getchar();

	// Restore VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);

	return 0;
}
