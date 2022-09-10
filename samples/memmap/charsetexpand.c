#include <c64/vic.h>
#include <c64/memmap.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <oscar.h>

// make space until 0xcc00 by extending the default region

#pragma region( main, 0x0a00, 0xcc00, , , {code, data, bss, heap, stack} )

// space for our custom charset from c000 to c800, will be copied to
// 0xd000 during startup to free space for stack and heap

#pragma section( charset, 0)

#pragma region( charset, 0xc000, 0xc800, , , {charset} )

// set initialized data segment to charset section

#pragma data(charset)

// lz compressed data
char charset[] = {
	#embed 2048 0 lzo "../resources/charset.bin"
};

// back to normal

#pragma data(data)

// pointers to charset and screen in memory

#define Screen	((char *)0xcc00)
#define Charset	((char *)0xd000)

int main(void)
{
	// Install the trampoline
	mmap_trampoline();

	// make all of RAM visibile to the CPU
	mmap_set(MMAP_RAM);

	// expand the font
	oscar_expand_lzo(Charset, charset);

	// make lower part of RAM visible to CPU
	mmap_set(MMAP_NO_BASIC);

	// map the vic to the new charset

	vic_setmode(VICM_TEXT, Screen, Charset);

	for(int i=0; i<1000; i++)
		Screen[i] = (char)i;

	// wait for keypress

	getchar();

	// restore VIC

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);

	// restore basic ROM
	mmap_set(MMAP_ROM);

    return 0;
}

