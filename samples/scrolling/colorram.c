#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdlib.h>

// Screen and color space
#define screen ((byte *)0x0400)
#define color ((byte *)0xd800)

// Macro for easy access to screen and color space
#define sline(x, y) (screen + 40 * (y) + (x))
#define cline(x, y) (color + 40 * (y) + (x))

// Column buffer for one prepared column of screen and color data
char rbuff[25], cbuff[25];

// Split into three scrolling sections to race the beam
#define SPLIT1	8
#define SPLIT2	16

// Scroll top section
void scrollLeft0(void)
{
	for(char x=0; x<39; x++)
	{
		#pragma unroll(full)
		for(char y=0; y<SPLIT1; y++)
		{
			sline(0, y)[x] = sline(1, y)[x];
			cline(0, y)[x] = cline(1, y)[x];
		}
	}

	#pragma unroll(full)
	for(char y=0; y<SPLIT1; y++)
	{
		sline(0, y)[39] = rbuff[y];
		cline(0, y)[39] = cbuff[y];
	}
}

// Scroll bottom two sections
void scrollLeft1(void)
{
	for(char x=0; x<39; x++)
	{
		#pragma unroll(full)
		for(char y=SPLIT1; y<SPLIT2; y++)
		{
			sline(0, y)[x] = sline(1, y)[x];
			cline(0, y)[x] = cline(1, y)[x];
		}
	}
	#pragma unroll(full)
	for(char y=SPLIT1; y<SPLIT2; y++)
	{
		sline(0, y)[39] = rbuff[y];
		cline(0, y)[39] = cbuff[y];
	}

	for(char x=0; x<39; x++)
	{
		#pragma unroll(full)
		for(char y=SPLIT2; y<25; y++)
		{
			sline(0, y)[x] = sline(1, y)[x];
			cline(0, y)[x] = cline(1, y)[x];
		}
	}
	#pragma unroll(full)
	for(char y=SPLIT2; y<25; y++)
	{
		sline(0, y)[39] = rbuff[y];
		cline(0, y)[39] = cbuff[y];
	}
}


// Prepare a new column with random data
void prepcol(void)
{
	for(char i=0; i<25; i++)
	{
		unsigned r = rand();
		cbuff[i] = r & 15;
		rbuff[i] = (r & 16) ? 102 : 160;
	}
}

int main(void)
{
	// Clear the screen
	memset(screen, 0x20, 1000);
	memset(color, 7, 1000);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	char	x = 0;

	for(;;)
	{
		// Advance one pixel

		x = (x + 1) & 7;

		// If we will cross the character boundary, scroll the top section
		if (x == 0)
		{
			// Wait for raster reaching bottom of first section
			vic_waitLine(50 + 8 * SPLIT1);
			// Scroll first section
			scrollLeft0();
		}

		// Wait for bottom of screen
		vic_waitBottom();

		// Update the pixel offset
		vic.ctrl2 = (7 - x) & 7;

		if (x == 0)
		{
			// Scroll the bottom section if needed
			scrollLeft1();
		}
		else 
		{
			// Update the new column somewhere in the middle of the character
			if (x == 4)
				prepcol();
			vic_waitTop();
		}
	}

	return 0;

}
