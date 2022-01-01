#include <c64/kernalio.h>
#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <stdio.h>

Bitmap	Screen;

#define ScreenMem	((char *)0xe000)
#define ColorMem	((char *)0xd000)

int main(void)
{
	// Install the IRQ trampoline
	mmap_trampoline();

	// Initialize the display bitmap
	bm_init(&Screen, ScreenMem, 40, 25);

	// Clear the color memory with ROM and IO disabled
	mmap_set(MMAP_RAM);
	memset(ScreenMem, 0, 8000);
	memset(ColorMem, 0x70, 1000);
	mmap_set(MMAP_NO_ROM);

	// Switch VIC to hires mode
	vic_setmode(VICM_HIRES, ColorMem, ScreenMem);

	// Reenable the kernal rom
	mmap_set(MMAP_ROM);


	// Set name for file and open it with replace on drive 9
	krnio_setnam("TESTIMAGE,P,R");	
	if (krnio_open(2, 9, 2))
	{
		// Read the bitmap image in one go
		krnio_read(2, ScreenMem, 8000);

		// Close the file
		krnio_close(2);
	}

	// Wait for a character
	getchar();

	// Restore VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);

	return 0;
}
