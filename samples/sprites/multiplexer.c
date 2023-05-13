#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <c64/sprites.h>
#include <math.h>

#define Screen ((char *)0x400)

// make space until 0x2000 for code and data

#pragma region( lower, 0x0a00, 0x2000, , , {code, data} )

// then space for our sprite data

#pragma section( spriteset, 0)

#pragma region( spriteset, 0x2000, 0x2800, , , {spriteset} )

// everything beyond will be code, data, bss and heap to the end

#pragma region( main, 0x2800, 0xa000, , , {code, data, bss, heap, stack} )


// spriteset at fixed location

#pragma data(spriteset)

char spriteset[2048] = {
	#embed "../resources/digitsprites.bin"
};

#pragma data(data)

// sinus table for circular movement

int		sintab[128];

int main(void)
{
	// calculate sine table
	for(int i=0; i<128; i++)
		sintab[i] = (int)(70 * sin(i * PI / 64));

	// enable raster interrupt via kernal path
	rirq_init(true);

	// initialize sprite multiplexer
	vspr_init(Screen);

	// initialize sprites
	for(int i=0; i<16; i++)
	{
		vspr_set(i, 30 + 16 * i, 220 - 8 * i, (unsigned)&(spriteset[0]) / 64 + i, (i & 7) + 8);
	}

	// initial sort and update
	vspr_sort();
	vspr_update();
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// animation loop
	unsigned j = 0;
	for(;;)
	{
		// move sprites around
		char	k = j >> 4;
		for(char i=0; i<16; i++)
		{
			vspr_move(i, 200 + sintab[(j + 8 * i) & 127] + sintab[k & 127], 150 + sintab[(j + 8 * i + 32) & 127] + sintab[(k + 32) & 127]);
		}
		j++;

		// sort virtual sprites by y position
		vspr_sort();

		// wait for raster IRQ to reach and of frame
		rirq_wait();

		// update sprites back to normal and set up raster IRQ for second set
		vspr_update();

		// sort raster IRQs
		rirq_sort();
	}

	return 0;

}
