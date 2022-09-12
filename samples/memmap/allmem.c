#include <c64/memmap.h>
#include <stdlib.h>
#include <stdio.h>

// make space until 0x1000 by for the stack

#pragma stacksize(0x0600)

#pragma region( stack, 0x0a00, 0x1000, , , {stack} )

// everything beyond will be code, data, bss and heap to the end

#pragma region( main, 0x1000, 0xfff0, , , {code, data, bss, heap} )

int main(void)
{
	// Install the IRQ trampoline

	mmap_trampoline();

	// Hide the basic ROM, must be first instruction

	mmap_set(MMAP_RAM);

	// Allocate all memory

	unsigned total = 0;
	while (char * data = malloc(1024))
	{
		total += 1024;

		// Swap in kernal for print

		mmap_set(MMAP_NO_BASIC);
		printf("ALLOCATED %5u AT %04x\n", total, (unsigned)data);
		mmap_set(MMAP_RAM);

		// Fill it with trash

		for(unsigned i=0; i<1024; i++)
			data[i] = 0xaa;
	}

	// Return basic ROM to normal state

	mmap_set(MMAP_ROM);

    return 0;
}

