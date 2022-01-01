#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <c64/joystick.h>
#include <stdlib.h>

// Screen and color space
#define Screen ((byte *)0x0400)
#define Color ((byte *)0xd800)

// Macro for easy access to screen and color space
#define sline(x, y) (Screen + 40 * (y) + (x))
#define cline(x, y) (Color + 40 * (y) + (x))

// Tile data, each column has four rows of four tiles
static const char quad[4][4 * 4] =
{
	{
		0x20, 0x55, 0x6c, 0x4e,
		0x20, 0x5d, 0xe1, 0x65,
		0x20, 0x5d, 0xe1, 0x65,
		0x20, 0x4a, 0x7c, 0x4d, 
	},
	{
		0x20, 0x40, 0x62, 0x77,
		0x20, 0x20, 0xa0, 0x20,
		0x20, 0x20, 0xa0, 0x20,
		0x20, 0x40, 0xe2, 0x6f,
	},
	{
		0x20, 0x40, 0x62, 0x77,
		0x20, 0x20, 0xa0, 0x20,
		0x20, 0x20, 0xa0, 0x20,
		0x20, 0x40, 0xe2, 0x6f,
	},
	{
		0x20, 0x49, 0x7b, 0x4d,
		0x20, 0x5d, 0x61, 0x6a,
		0x20, 0x5d,	0x61, 0x6a,
		0x20, 0x4b, 0x7e, 0x4e,
	}
};


#pragma align(quad, 256)

// Expand a single row into an offscreen buffer
void expandrow(char * dp, char * cp, const char * grid, char ly, char lx)
{
	char	hx = 0;
	for(char x=0; x<40; x++)
	{
		dp[x] = quad[lx][ly + grid[hx]];
		cp[x] = grid[hx];
		lx++;
		if (lx == 4)
		{
			lx = 0;
			hx++;
		}
	}
}

// Expand a single column into an offscreen buffer
void expandcol(char * dp, char * cp, const char * grid, char ly, char lx)
{
	for(char y=0; y<25; y++)
	{
		dp[y] = quad[lx][ly + grid[0]];
		cp[y] = grid[0];
		ly += 4;
		if (ly == 16)
		{
			grid += 16;
			ly = 0;
		}
	}
}

// Two split scroll for left, right and up
#define VSPLIT	12

// Three split scroll for down.  Downscrolling is more tricky because
// we have to copy towards the raster
#define VSPLIT	12
#define VSPLIT2	20

// New line/column of screen and color data
char	news[40], newc[40];

// All scroll routines start with a pixel offset of 4, 4

// Scroll one character left in two pixel increments
void scroll_left(void)
{
	// Wait for one frame
	vic_waitTop();
	vic_waitBottom();

	// Switch to offset 2, 4
	vic.ctrl2 = 0x02;
	vic_waitTop();
	vic_waitBottom();

	// Switch to offset 0, 4
	vic.ctrl2 = 0x00;

	// Wait until bottom of section
	vic_waitLine(50 + 8 * VSPLIT);

	// Scroll upper section

	for(char x=0; x<39; x++)
	{
#assign ty 0
#repeat
		sline(0, ty)[x] = sline(1, ty)[x];
		cline(0, ty)[x] = cline(1, ty)[x];
#assign ty ty + 1
#until ty == VSPLIT
	}

	// Update column
#assign ty 0
#repeat
	sline(0, ty)[39] = news[ty];
	cline(0, ty)[39] = newc[ty];
#assign ty ty + 1
#until ty == VSPLIT

	// Wait for bottom of visible screen
	vic_waitBottom();

	// Switch to offset 6, 4
	vic.ctrl2 = 0x06;

	// Scroll lower part of the screen, while top is redrawn
	for(char x=0; x<39; x++)
	{
#assign ty VSPLIT
#repeat
		sline(0, ty)[x] = sline(1, ty)[x];
		cline(0, ty)[x] = cline(1, ty)[x];
#assign ty ty + 1
#until ty == 25
	}

	// Update new column
#assign ty VSPLIT
#repeat
	sline(0, ty)[39] = news[ty];
	cline(0, ty)[39] = newc[ty];
#assign ty ty + 1
#until ty == 25

	// Wait for bottom
	vic_waitBottom();

	// Now back to 4, 4
	vic.ctrl2 = 0x04

}

// Scroll one character right in two pixel increments

void scroll_right(void)
{
	vic_waitTop();
	vic_waitBottom();

	vic.ctrl2 = 0x06;
	vic_waitLine(50 + 8 * VSPLIT);

	for(char x=39; x>0; x--)
	{
#assign ty 0
#repeat
		sline(0, ty)[x] = sline(-1, ty)[x];
		cline(0, ty)[x] = cline(-1, ty)[x];
#assign ty ty + 1
#until ty == VSPLIT
	}

#assign ty 0
#repeat
	sline(0, ty)[0] = news[ty];
	cline(0, ty)[0] = newc[ty];
#assign ty ty + 1
#until ty == VSPLIT

	vic_waitBottom();
	vic.ctrl2 = 0x00;

	for(char x=39; x>0; x--)
	{
#assign ty VSPLIT
#repeat
		sline(0, ty)[x] = sline(-1, ty)[x];
		cline(0, ty)[x] = cline(-1, ty)[x];
#assign ty ty + 1
#until ty == 25
	}

#assign ty VSPLIT
#repeat
	sline(0, ty)[0] = news[ty];
	cline(0, ty)[0] = newc[ty];
#assign ty ty + 1
#until ty == 25

	vic_waitBottom();
	vic.ctrl2 = 0x02

	vic_waitTop();
	vic_waitBottom();
	vic.ctrl2 = 0x04;
}

// Scroll one character up in two pixel increments

void scroll_up(void)
{
	vic_waitTop();
	vic_waitBottom();

	vic.ctrl1 = 0x02 | VIC_CTRL1_DEN;
	vic_waitTop();
	vic_waitBottom();

	vic.ctrl1 = 0x00 | VIC_CTRL1_DEN;
	vic_waitLine(50 + 8 * VSPLIT);

	for(char x=0; x<40; x++)
	{
#assign ty 0
#repeat
		sline(0, ty)[x] = sline(0, ty + 1)[x];
		cline(0, ty)[x] = cline(0, ty + 1)[x];
#assign ty ty + 1
#until ty == VSPLIT
	}

	vic_waitBottom();
	vic.ctrl1 = 0x06 | VIC_CTRL1_DEN;

	for(char x=0; x<40; x++)
	{
#assign ty VSPLIT
#repeat
		sline(0, ty)[x] = sline(0, ty + 1)[x];
		cline(0, ty)[x] = cline(0, ty + 1)[x];
#assign ty ty + 1
#until ty == 24

		sline(0, ty)[x] = news[x];
		cline(0, ty)[x] = newc[x];
	}

	vic_waitBottom();
	vic.ctrl1 = 0x04 | VIC_CTRL1_DEN;
}

char	tmp0[40], tmp1[40], tmp2[40], tmp3[40];

// Scroll one character down in two pixel increments.  This is more tricky than
// the other three cases, because we have to work towards the beam because
// we have to copy backwards in memory.
// 
// The scroll is split into three sections, the seam rows are saved into
// intermediate arrays, so we can copy the top section first and the bottom
// section last, and stay ahead of the beam.

void scroll_down(void)
{
	// Wait one frame
	vic_waitTop();
	vic_waitBottom();

	// Save seam lines
	for(char x=0; x<40; x++)
	{
		tmp0[x] = sline(0, VSPLIT)[x];
		tmp1[x] = cline(0, VSPLIT)[x];
		tmp2[x] = sline(0, VSPLIT2)[x];
		tmp3[x] = cline(0, VSPLIT2)[x];
	}

	// Now switch to 4, 6
	vic.ctrl1 = 0x06 | VIC_CTRL1_DEN;

	// Wait for bottom of top section
	vic_waitLine(58 + 8 * VSPLIT);

	// Scroll top section down and copy new column
	for(char x=0; x<40; x++)
	{
#assign ty VSPLIT
#repeat
		sline(0, ty)[x] = sline(0, ty - 1)[x];
		cline(0, ty)[x] = cline(0, ty - 1)[x];
#assign ty ty - 1
#until ty == 0

		sline(0, ty)[x] = news[x];
		cline(0, ty)[x] = newc[x];		
	}

//	vic_waitBottom();
	// We have already reached the bottom, switch to 4, 0
	vic.ctrl1 = 0x00 | VIC_CTRL1_DEN;

	// Copy the second section, update the seam line from the buffer
	for(char x=0; x<40; x++)
	{

#assign ty VSPLIT2
#repeat
		sline(0, ty)[x] = sline(0, ty - 1)[x];
		cline(0, ty)[x] = cline(0, ty - 1)[x];
#assign ty ty - 1
#until ty == VSPLIT + 1

		sline(0, ty)[x] = tmp0[x];
		cline(0, ty)[x] = tmp1[x];
	}

	// Copy the third section, update the seam line from the buffer
	for(char x=0; x<40; x++)
	{
#assign ty 24
#repeat
		sline(0, ty)[x] = sline(0, ty - 1)[x];
		cline(0, ty)[x] = cline(0, ty - 1)[x];
#assign ty ty - 1
#until ty == VSPLIT2 + 1

		sline(0, ty)[x] = tmp2[x];
		cline(0, ty)[x] = tmp3[x];
	}

	// Switch to 4, 2
	vic_waitBottom();
	vic.ctrl1 = 0x02 | VIC_CTRL1_DEN;

	// Switch to 4, 4
	vic_waitTop();
	vic_waitBottom();
	vic.ctrl1 = 0x04 | VIC_CTRL1_DEN;
}

char grid[16][16];

#pragma align(grid, 256)

int main(void)
{
	// We need some more accurate timing for this, so kill the kernal IRQ
	__asm
	{
		sei
	}

	// Init the grid
	for(char y=0; y<16; y++)
	{
		for(char x=0; x<16; x++)
		{
			grid[y][x] = rand() & 3;
		}
	}

	char	gridX = 0, gridY = 0;

	// Inital drwaing of the screen
	char	*	dp = Screen, * cp = Color;
	for(char y=0; y<25; y++)
	{
		expandrow(dp, cp, &(grid[y >> 2][0]), 4 * (y & 3), 0);
		dp += 40;
		cp += 40;
	}

	// setup initial scroll offset
	
	vic.ctrl1 = 0x04 | VIC_CTRL1_DEN;
	vic.ctrl2 = 0x04

	for(;;)
	{
		// Check the joystick
		joy_poll(1);
		if (joyx[1] == 1)
		{
			// Move to the right
			if (gridX < 24)
			{
				gridX++;
				expandcol(news, newc, &(grid[gridY >> 2][(gridX + 39) >> 2]), 4 * (gridY & 3), (gridX + 39) & 3);
				scroll_left();
			}
		}
		else if (joyx[1] == -1)
		{
			// Move to the left
			if (gridX > 0)
			{
				gridX--;
				expandcol(news, newc, &(grid[gridY >> 2][gridX >> 2]), 4 * (gridY & 3), gridX & 3);
				scroll_right();
			}
		}
		else if (joyy[1] == 1)
		{
			// Move down
			if (gridY < 39)
			{
				gridY++;
				expandrow(news, newc, &(grid[(gridY + 24) >> 2][gridX >> 2]), 4 * ((gridY + 24) & 3), gridX & 3);
				scroll_up();
			}
		}
		else if (joyy[1] == -1)
		{
			// Move up
			if (gridY > 0)
			{
				gridY--;
				expandrow(news, newc, &(grid[gridY >> 2][gridX >> 2]), 4 * (gridY & 3), gridX & 3);
				scroll_down();
			}
		}
	}

	return 0;
}
