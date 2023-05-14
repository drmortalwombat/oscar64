#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/rasterirq.h>
#include <c64/memmap.h>
#include <c64/cia.h>

static const sbyte sintab[256] = {
	0, 2, 4, 7, 9, 11, 13, 15, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 55, 57, 59, 60, 
	62, 64, 65, 67, 68, 70, 71, 72, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 85, 86, 87, 87, 88, 88, 89, 89, 89, 90, 90, 90, 90, 
	90, 90, 90, 90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 85, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 72, 71, 70, 68, 67, 65, 64, 
	62, 60, 59, 57, 55, 54, 52, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 15, 13, 11, 9, 7, 4, 2, 
	0, -2, -4, -7, -9, -11, -13, -15, -18, -20, -22, -24, -26, -28, -30, -32, -34, -36, -38, -40, -42, -44, -46, -48, -50, -52, -54, -55, -57, -59, -60, 
	-62, -64, -65, -67, -68, -70, -71, -72, -74, -75, -76, -77, -78, -79, -80, -81, -82, -83, -84, -85, -85, -86, -87, -87, -88, -88, -89, -89, -89, -90, -90, -90, -90, 
	-90, -90, -90, -90, -90, -89, -89, -89, -88, -88, -87, -87, -86, -85, -85, -84, -83, -82, -81, -80, -79, -78, -77, -76, -75, -74, -72, -71, -70, -68, -67, -65, -64, 
	-62, -60, -59, -57, -55, -54, -52, -50, -48, -46, -44, -42, -40, -38, -36, -34, -32, -30, -28, -26, -24, -22, -20, -18, -15, -13, -11, -9, -7, -4, -2
};

static const sbyte costab[256] = {
	120, 120, 120, 120, 119, 119, 119, 118, 118, 117, 116, 116, 115, 114, 113, 112, 111, 110, 108, 107, 106, 104, 103, 101, 100, 98, 96, 95, 93, 91, 89, 87, 
	85, 83, 81, 78, 76, 74, 71, 69, 67, 64, 62, 59, 57, 54, 51, 49, 46, 43, 40, 38, 35, 32, 29, 26, 23, 21, 18, 15, 12, 9, 6, 3, 
	0, -3, -6, -9, -12, -15, -18, -21, -23, -26, -29, -32, -35, -38, -40, -43, -46, -49, -51, -54, -57, -59, -62, -64, -67, -69, -71, -74, -76, -78, -81, -83, 
	-85, -87, -89, -91, -93, -95, -96, -98, -100, -101, -103, -104, -106, -107, -108, -110, -111, -112, -113, -114, -115, -116, -116, -117, -118, -118, -119, -119, -119, -120, -120, -120, 
	-120, -120, -120, -120, -119, -119, -119, -118, -118, -117, -116, -116, -115, -114, -113, -112, -111, -110, -108, -107, -106, -104, -103, -101, -100, -98, -96, -95, -93, -91, -89, -87, 
	-85, -83, -81, -78, -76, -74, -71, -69, -67, -64, -62, -59, -57, -54, -51, -49, -46, -43, -40, -38, -35, -32, -29, -26, -23, -21, -18, -15, -12, -9, -6, -3, 
	0, 3, 6, 9, 12, 15, 18, 21, 23, 26, 29, 32, 35, 38, 40, 43, 46, 49, 51, 54, 57, 59, 62, 64, 67, 69, 71, 74, 76, 78, 81, 83, 
	85, 87, 89, 91, 93, 95, 96, 98, 100, 101, 103, 104, 106, 107, 108, 110, 111, 112, 113, 114, 115, 116, 116, 117, 118, 118, 119, 119, 119, 120, 120, 120
};

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

static const char spriteset[2048] = {
	#embed "../resources/digitsprites.bin"
};

#pragma data(data)


int main(void)
{
	// Disable interrupts while setting up
	__asm { sei };

	// Kill CIA interrupts
	cia_init();

	mmap_set(MMAP_NO_ROM);

	// enable raster interrupt via direct path
	rirq_init(false);

	// initialize sprite multiplexer
	vspr_init(Screen);

	// initialize sprites
	for(char i=0; i<32; i++)
	{
		vspr_set(i, 30 + 8 * i, 220 - 4 * i, (unsigned)&(spriteset[0]) / 64 + (i & 15), (i & 7) + 8);
	}

	// initial sort and update
	vspr_sort();
	vspr_update();
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// Black screen
	vic.color_border = 0;
	vic.color_back = 0;

	// animation loop
	unsigned j = 0, t = 0;
	unsigned k = 91;
	for(;;)
	{
		// Use MSB as start position
		j = t << 8;

		// Unroll movement loop for performance
		#pragma unroll(full)
		for(char i=0; i<32; i++)
		{
			vspr_move(i, 170 + costab[((j >> 8) + 8 * i) & 255], 140 + sintab[(t + 8 * i) & 255]);
			j += k;
		}

		// Advance animation
		t+=3;
		k++;

		// sort virtual sprites by y position
		vspr_sort();

		// wait for raster IRQ to reach and of frame
		rirq_wait();

		// update sprites back to normal and set up raster IRQ for sprites 8 to 31
		vspr_update();

		// sort raster IRQs
		rirq_sort();

	}

	return 0;
}
