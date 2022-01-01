#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdlib.h>

// Screen and color space
#define screen ((byte *)0x0400)
#define color ((byte *)0xd800)

// Macro for easy access to screen space
#define sline(x, y) (screen + 40 * (y) + (x))

// Column buffer for one prepared column of tunnel
char rbuff[25];

// Copy the prepared tunnel column to screen
void expand(char x)
{
	// Unroll for each row
#assign y 0		
#repeat
	sline(0, y)[x] = rbuff[y];
#assign y y + 1
#until y == 25
}

// Scrolling left, copying new column.  This is split into two
// unrolled sections so the update of the new column can race the
// beam

void scrollLeft(void)
{
	// First 12 rows scroll left and copy new column
	for(char x=0; x<39; x++)
	{
#assign y 0		
#repeat
		sline(0, y)[x] = sline(1, y)[x];
#assign y y + 1
#until y == 12
	}
#assign y 0		
#repeat
	sline(0, y)[39] = rbuff[y];
#assign y y + 1
#until y == 12

	// Final 13 rows scroll left and copy new column
	for(char x=0; x<39; x++)
	{
#assign y 12	
#repeat
		sline(0, y)[x] = sline(1, y)[x];
#assign y y + 1
#until y == 25
	}
#assign y 12	
#repeat
	sline(0, y)[39] = rbuff[y];
#assign y y + 1
#until y == 25
}

// Scrolling right, copying new column.  This is split into two
// unrolled sections so the update of the new column can race the
// beam

void scrollRight(void)
{
	for(char x=39; x>0; x--)
	{
#assign y 0		
#repeat
		sline(0, y)[x] = sline(-1, y)[x];
#assign y y + 1
#until y == 12
	}
#assign y 0		
#repeat
	sline(0, y)[0] = rbuff[y];
#assign y y + 1
#until y == 12

	for(char x=39; x>0; x--)
	{
#assign y 12	
#repeat
		sline(0, y)[x] = sline(-1, y)[x];
#assign y y + 1
#until y == 25
	}	
#assign y 12	
#repeat
	sline(0, y)[0] = rbuff[y];
#assign y y + 1
#until y == 25
}

// Top and bottom row of the tunnel

char ytop[256], ybottom[256];

// Prepare one column of the tunnel

void prepcol(char xi)
{
	char		yt, yb;
	signed char dyt, dyb;

	// Current height of top and bottom
	yt = ytop[(char)(xi + 0)];
	yb = ybottom[(char)(xi + 0)];

	// Height of column to the left for diagonal
	dyt = yt - ytop[(char)(xi - 1)];
	dyb = yb - ybottom[(char)(xi - 1)];				

	// Fill top, center and bottom range
	for(char i=0; i<yt; i++)
		rbuff[i] = 160;
	for(char i=yt; i<yb; i++)
		rbuff[i] = 32;
	for(char i=yb; i<25; i++)
		rbuff[i] = 160;

	// Select transitional characters based on slope
	if (dyt < 0)
		rbuff[yt] = 105;
	else if (dyt > 0)
		rbuff[yt - 1] = 95;

	if (dyb < 0)
		rbuff[yb] = 233;
	else if (dyb > 0)
		rbuff[yb - 1] = 223;

}

// Initialize tunnel with "random" data
void buildTunnel(void)
{
	signed char yt = 1, yb = 24, dyt = 1, dyb = -1;

	for(int i=0; i<256; i++)
	{
		unsigned r = rand();

		if (!(r & 0x00e0))
			dyt = -dyt;
		if (!(r & 0xe000))
			dyb = -dyb;

		yt += dyt;
		yb += dyb;
		if (yt < 0)
		{
			yt = 0;
			dyt = 1;
		}
		if (yb > 25)
		{
			yb = 25;
			dyb = -1;
		}

		ytop[i] = yt;
		ybottom[i] = yb;

		if (yt + 5 > yb)
		{
			dyt = -1;
			dyb = 1;
		}
	}

}

int main(void)
{
	// Clear the screen

	memset(screen, 0x20, 1000);
	memset(color, 7, 1000);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	// Build tunnel

	buildTunnel();

	// Initial fill of screen

	for(char i=0; i<40; i++)
	{
		prepcol(i);
		expand(i);
	}

	// Now start moving
	int	xpos = 0, dx = 0, ax = 1;
	int	xi = 0, pxi = 0;

	for(;;)
	{
		// Random change of direction
		unsigned r = rand();
		if ((r & 127) == 0)
			ax = -ax;

		// Acceleration
		dx += ax;
		if (dx > 32)
			dx = 32;
		else if (dx < -32)
			dx = -32;

		// Movement
		xpos += dx;
		pxi = xi;
		xi = xpos >> 5;

		// Check if we cross a character boundary, and if so prepare
		// the new column
		if (pxi < xi)
			prepcol(xi + 39);
		else if (pxi > xi)
			prepcol(xi + 0);

		// Wait one frame
		vic_waitTop();
		vic_waitBottom();

		// Update pixel level scrolling
		vic.ctrl2 = (7 - (xpos >> 2)) & 7;

		// Character level scrolling if needed
		if (pxi < xi)
			scrollLeft();
		else if (pxi > xi)
			scrollRight();
	}

	return 0;

}
