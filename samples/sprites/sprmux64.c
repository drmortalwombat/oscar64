#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <c64/memmap.h>
#include <c64/cia.h>

static const sbyte costab[256] = {
	30, 30, 30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 28, 28, 28, 28, 27, 27, 27, 26, 26, 26, 25, 25, 25, 24, 24, 23, 23, 22, 22, 
	21, 21, 20, 20, 19, 18, 18, 17, 17, 16, 15, 15, 14, 13, 13, 12, 11, 11, 10, 9, 9, 8, 7, 7, 6, 5, 4, 4, 3, 2, 1, 1, 
	0, -1, -1, -2, -3, -4, -4, -5, -6, -7, -7, -8, -9, -9, -10, -11, -11, -12, -13, -13, -14, -15, -15, -16, -17, -17, -18, -18, -19, -20, -20, 
	-21, -21, -22, -22, -23, -23, -24, -24, -25, -25, -25, -26, -26, -26, -27, -27, -27, -28, -28, -28, -28, -29, -29, -29, -29, -29, -30, -30, -30, -30, -30, -30, -30, 
	-30, -30, -30, -30, -30, -30, -30, -30, -29, -29, -29, -29, -29, -28, -28, -28, -28, -27, -27, -27, -26, -26, -26, -25, -25, -25, -24, -24, -23, -23, -22, -22, 
	-21, -21, -20, -20, -19, -18, -18, -17, -17, -16, -15, -15, -14, -13, -13, -12, -11, -11, -10, -9, -9, -8, -7, -7, -6, -5, -4, -4, -3, -2, -1, -1, 
	0, 1, 1, 2, 3, 4, 4, 5, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15, 16, 17, 17, 18, 18, 19, 20, 20, 
	21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 29, 29, 30, 30, 30, 30, 30, 30, 30
};

#define Screen ((char *)0x400)

// make space until 0x2000 for code and data

#pragma region( lower, 0x0a00, 0x2000, , , {code, data} )

// then space for our sprite data

#pragma section( spriteset, 0)

#pragma region( spriteset, 0x2000, 0x2040, , , {spriteset} )

// everything beyond will be code, data, bss and heap to the end

#pragma region( main, 0x2800, 0xa000, , , {code, data, bss, heap, stack} )


// spriteset at fixed location

#pragma data(spriteset)

__export const char spriteset[64] = {
	#embed "../resources/ballsprite.bin"
};

#pragma data(data)

// Control variables for the interrupt

char 	pphase;		// global phase
char	poffset;	// phase offset for this row of sprites
char	yoffset;	// vertical offset for this row of sprites

// Interrupt routine switching to next row of sprites, invokes some lines after
// start of previous row

__interrupt void setspr(void)
{
	// Switch vertical position first, will not take effect
	// until after the current row of sprites is complete, so it
	// is done first
	#pragma unroll(full)
	for(char i=0; i<8; i++)
		vic.spr_pos[i].y = yoffset;

	char phase = pphase + poffset;		// Effective rotation phase for this row
	int step = costab[phase];			// 30 * Cosine of phase for x step
	int xpos0 = 172 - (step >> 1);		// Half a step to the left
	int xpos1 = 172 + (step >> 1);		// Half a step to the right

	// Table for xpositions
	static unsigned xp[8];

	// Calculate xpositions, four to the left and four to the right
	#pragma unroll(full)
	for(char i=0; i<4; i++)
	{
		xp[3 - i] = xpos0;
		xp[i + 4] = xpos1;

		// Stepping left and right
		xpos0 -= step;
		xpos1 += step;
	}

	// Wait for end of current sprite, xpos will take effect
	// at start of line, so we need to patch it after the last
	// pixel line has started
	vic_waitLine(yoffset - 4);

	// Left to right or right to left to get a matching z order
	if (phase & 0x80)
	{
		// MSB mask
		char	xymask = 0;

		// Update all sprite x LSB and color, put MSB into
		// xymask bit
		#pragma unroll(full)
		for(char i=0; i<8; i++)
		{
			xymask = ((unsigned)xymask | (xp[i] & 0xff00)) >> 1;
			vic.spr_pos[i].x = xp[i];
			vic.spr_color[i] = VCOL_ORANGE + i;
		}

		// Update MSB
		vic.spr_msbx = xymask;
	}
	else
	{
		char	xymask = 0;

		// Update all sprite x LSB and color, put MSB into
		// xymask bit

		#pragma unroll(full)
		for(char i=0; i<8; i++)
		{
			xymask = ((unsigned)xymask | (xp[7 - i] & 0xff00)) >> 1;
			vic.spr_pos[i].x = xp[7 - i];
			vic.spr_color[i] = VCOL_ORANGE + (7 - i);
		}

		// Update MSB
		vic.spr_msbx = xymask;		
	}
}

// Eight raster interrupts
RIRQCode	spmux[8];

int main(void)
{
	// Keep kernal alive 
	rirq_init(true);

	// Setup sprite images
	for(char i=0; i<8; i++)
		Screen[0x03f8 + i] = 128;

	// Remaining sprite registers
	vic.spr_enable = 0xff;
	vic.spr_multi = 0xff;
	vic.spr_expand_x = 0x00;
	vic.spr_expand_y = 0x00;
	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Setup raster interrupts
	for(char i=0; i<8; i++)
	{
		// Three operations per interrupt
		rirq_build(spmux + i, 3);
		// Store phase offset
		rirq_write(spmux + i, 0, &poffset, 11 * i);
		// Store vertical position
		rirq_write(spmux + i, 1, &yoffset, 52 + 25 * i);
		// Call sprite update function
		rirq_call(spmux + i, 2, setspr);

		// Place raster interrupt 16 lines before sprite start to
		// give it enough time for procesing
		rirq_set(i, 36 + 25 * i, spmux + i);
	}

	// Sort interrupts and start processing
	rirq_sort();
	rirq_start();

	// Forever
	for(;;)
	{
		// Advance phase
		pphase += 5;

		// Wait for next frame
		vic_waitFrame();
	}

	return 0;
}
