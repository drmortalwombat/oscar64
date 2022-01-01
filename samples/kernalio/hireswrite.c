#include <c64/kernalio.h>
#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <stdio.h>

Bitmap	Screen, Brush;
char	Buffer[200];

#define ScreenMem	((char *)0xe000)
#define ColorMem	((char *)0xd000)

int main(void)
{
	// Install the IRQ trampoline
	mmap_trampoline();

	// Initialize the display bitmap and a brush
	bm_init(&Screen, ScreenMem, 40, 25);
	bm_alloc(&Brush, 2, 2);

	// Clear the color memory with ROM and IO disabled
	mmap_set(MMAP_RAM);
	memset(ScreenMem, 0, 8000);
	memset(ColorMem, 0x70, 1000);
	mmap_set(MMAP_NO_ROM);

	// Switch VIC to hires mode
	vic_setmode(VICM_HIRES, ColorMem, ScreenMem);

	// Draw the brush
	ClipRect	crb = {0, 0, 16, 16};
	bm_fill(&Brush, 0);
	bm_circle_fill(&Brush, &crb, 7, 7, 6, NineShadesOfGrey[8]);

	// Draw the main image
	ClipRect	crr = {0, 0, 320, 200};
	bm_circle_fill(&Screen, &crr, 160, 100, 90, NineShadesOfGrey[8]);
	bm_circle_fill(&Screen, &crr, 120,  80, 20, NineShadesOfGrey[0]);
	bm_circle_fill(&Screen, &crr, 200,  80, 20, NineShadesOfGrey[0]);

	// And a smile
	for(int x=-40; x<=40; x+=4)
	{
		int y = bm_usqrt(50 * 50 - x * x);		
		bm_bitblit(&Screen, &crr, 160 - 7 + x, 100 + y, &Brush, 0, 0, 15, 15, nullptr, BLTOP_AND_NOT);
	}


	// Reenable the kernal rom
	mmap_set(MMAP_ROM);

	// Set name for file and open it with replace on drive 9
	krnio_setnam("@0:TESTIMAGE,P,W");	
	if (krnio_open(2, 9, 2))
	{
		// Loop in chunks of 200 bytes
		for(int i=0; i<8000; i+=200)
		{
			// Disable ROM
			mmap_set(MMAP_NO_ROM);
			// Copy chunk into buffer
			memcpy(Buffer, ScreenMem + i, 200);
			// Reeable the ROM
			mmap_set(MMAP_ROM);

			// Write the chunk to disk
			krnio_write(2, Buffer, 200);
		}

		// Close the file
		krnio_close(2);
	}

	// Restore the VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);

	return 0;
}
