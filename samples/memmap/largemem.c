#include <c64/memmap.h>
#include <stdlib.h>
#include <stdio.h>

// make space until 0xd000 by extending the default region

#pragma region( main, 0x0a00, 0xd000, , , {code, data, bss, heap, stack} )

int main(void)
{
	// Hide the basic ROM, must be first instruction

	mmap_set(MMAP_NO_BASIC);

	// Allocate all memory

	unsigned total = 0;
	while (char * data = malloc(1024))
	{
		total += 1024;

		printf("ALLOCATED %5u AT %04x\n", total, (unsigned)data);

		// Fill it with trash

		for(unsigned i=0; i<1024; i++)
			data[i] = 0xaa;
	}

	// Return basic ROM to normal state

	mmap_set(MMAP_ROM);

    return 0;
}
