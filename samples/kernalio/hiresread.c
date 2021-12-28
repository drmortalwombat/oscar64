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
	mmap_trampoline();

	bm_init(&Screen, ScreenMem, 40, 25);

	mmap_set(MMAP_RAM);
	memset(ScreenMem, 0, 8000);
	memset(ColorMem, 0x70, 1000);
	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES, ColorMem, ScreenMem);

	mmap_set(MMAP_ROM);
	krnio_setnam("TESTIMAGE,P,R");	
	if (krnio_open(2, 9, 2))
	{
		krnio_read(2, ScreenMem, 8000);
		krnio_close(2);
	}

	getchar();

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);

	return 0;
}
