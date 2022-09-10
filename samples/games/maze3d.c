#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Moving the VIC into the third bank and making
// room for double buffering+

byte * const Screen0 = (byte *)0xc800;
byte * const Screen1 = (byte *)0xcc00;
byte * const Font = (byte *)0xd000;
byte * const Color = (byte *)0xd800;
byte * const Color1 = (byte *)0xc400;

// Just to get started a mini maze
static const char * maze[16] = 
{
	"################",
	"#..........#...#",
	"#.###.####.###.#",
	"#........#.#.#.#",
	"##.#####.###.#.#",
	"#..#......##.#.#",
	"#.##.####......#",
	"#.#.....#.####.#",
	"#.#####.#....#.#",
	"#.......######.#",
	"#.##.####......#",
	"#.#..##...####.#",
	"#.##....#....#.#",
	"#.#####.######.#",
	"#..............#",
	"################",
};

// Character set including some ramps for the upper edge
char charset[2048] = {
	#embed "../resources/maze3dchars.bin"
};

// Current target screen
char * DrawScreen;

// Current target index
bool FlipIndex;

// Position and direction inside the maze
sbyte	px = 1, py = 3, dx = 1, dy = 0;

// Distance of blocks to the side, relative to the center ot the screen
// for full and half step
static const char zxdist0[] = {18, 6, 4, 3, 2, 1, 0};
static const char zxdist1[] = { 9, 5, 3, 2, 1, 0, 0};


// Flip double buffer, copying the color ram

void screen_flip(void)
{
	// Change idnex of screen
	FlipIndex = !FlipIndex;

	// Wait until raster beam reaches bottom
	vic_waitBottom();

	// Change vic start address
	vic_setmode(VICM_TEXT, FlipIndex ? Screen0 : Screen1, Font);

	// Copy the color ram in four chunks, to avoid
	// colliding with the beam

	char i = 0;
	do {
		Color[0x000 + i] = Color1[0x000 + i];
		i++;
	} while (i != 0);
	do {
		Color[0x100 + i] = Color1[0x100 + i];
		i++;
	} while (i != 0);
	do {
		Color[0x200 + i] = Color1[0x200 + i];
		i++;
	} while (i != 0);
	do {
		Color[0x300 + i] = Color1[0x300 + i];
		i++;
	} while (i != 0);

	// Change target buffer for next frame
	DrawScreen = FlipIndex ? Screen1 : Screen0;
}

// Rotating left or right is simulated by scrolling.  For
// performance reasons, we have four different scroll routines
// due to two buffers and two directions

// Loop over the two buffers

#assign si 0
#repeat

// other buffer
#assign ri 1 - si

// pointers of the two screen buffers, from and to
#define dst Screen##si
#define src Screen##ri

// scroll left Screen0 or Screen1
void screen_left_##si(void)
{
	for(char sx=0; sx<40; sx+=4)
	{
		// Wait for the beam to be just below the first line
		vic_waitLine(58);

		// Unroll for each row of screen and color ram
#assign ry 0
#repeat		
		// Copy one row by four chars
		for(char x=0; x<36; x++)
		{
			dst[40 * ry + x] = dst[40 * ry + x + 4];
			Color[40 * ry + x] = Color[40 * ry + x + 4];
		}

		// Fill in new screen and color data
		dst[40 * ry + 36] = src[40 * ry + sx + 0];
		dst[40 * ry + 37] = src[40 * ry + sx + 1];
		dst[40 * ry + 38] = src[40 * ry + sx + 2];
		dst[40 * ry + 39] = src[40 * ry + sx + 3];

		Color[40 * ry + 36] = Color1[40 * ry + sx + 0];
		Color[40 * ry + 37] = Color1[40 * ry + sx + 1];
		Color[40 * ry + 38] = Color1[40 * ry + sx + 2];
		Color[40 * ry + 39] = Color1[40 * ry + sx + 3];

		// repeat for each row
#assign ry ry + 1
#until ry == 25
#undef ry
	}
}

// scroll right Screen0 or Screen1
void screen_right_##si(void)
{
	for(char sx=40; sx>0; sx-=4)
	{
		vic_waitLine(58);
#assign ry 0
#repeat		
		for(char x=39; x>=4; x--)
		{
			dst[40 * ry + x] = dst[40 * ry - 4 + x];
			Color[40 * ry + x] = Color[40 * ry - 4 + x];
		}

		dst[40 * ry + 0] = src[40 * ry + sx - 4];
		dst[40 * ry + 1] = src[40 * ry + sx - 3];
		dst[40 * ry + 2] = src[40 * ry + sx - 2];
		dst[40 * ry + 3] = src[40 * ry + sx - 1];

		Color[40 * ry + 0] = Color1[40 * ry + sx - 4];
		Color[40 * ry + 1] = Color1[40 * ry + sx - 3];
		Color[40 * ry + 2] = Color1[40 * ry + sx - 2];
		Color[40 * ry + 3] = Color1[40 * ry + sx - 1];
#assign ry ry + 1
#until ry == 25
#undef ry
	}
}

#assign si si + 1
#until si == 2
#undef si
#undef ri

// Scroll current screen left
void screen_left(void)
{
	if (FlipIndex)
		screen_left_0();
	else
		screen_left_1();
}

// Scroll current screen right
void screen_right(void)
{
	if (FlipIndex)
		screen_right_0();
	else
		screen_right_1();
}

// Fill one color column
void color_column(char cx, char color)
{
#assign ry 0
#repeat		
	Color1[40 * ry + cx] = color;
#assign ry ry + 1
#until ry == 25
#undef ry
}

// Fill one screen column
void screen_column(char cx, char sx, char tc, char mc, char bc)
{
	// Calculate top and bottom row
	char	ty = sx / 4;
	char	by = 25 - sx;

	// Target pointer
	char	*	dp = DrawScreen + cx;

	// Check for non empty column
	if (by > ty)
	{
		// Space above
		for(char cy=0; cy<ty; cy++)
		{
			*dp = 126; dp += 40;
		}

		// Top element
		*dp = tc; dp += 40;

		// Wall body
		char n = by - ty - 2;
		for(char cy=0; cy<n; cy++)
		{
			*dp = mc; dp += 40;
		}

		// Bottom element
		*dp = bc; dp += 40;

		// Space below
		for(char cy=by; cy<25; cy++)
		{
			*dp = 126; dp += 40;
		}
	}
	else
	{
		// Special case, clear column
		for(char cy=0; cy<25; cy++)
		{
			*dp = 126; dp += 40;
		}		
	}
}

// Draw the current maze using the given z/x distance array
void maze_draw(const char * zxdist)
{
	// pick colors based on orientation
	char	cleft, cright, cfront;
	if (dx)
	{
		cfront = VCOL_MED_GREY;
		if (dx < 0)
		{
			cleft = VCOL_LT_GREY;
			cright = VCOL_DARK_GREY;
		}
		else
		{
			cleft = VCOL_DARK_GREY;
			cright = VCOL_LT_GREY;
		}
	}
	else
	{
		cleft = cright = VCOL_MED_GREY;
		cfront = dy > 0 ? VCOL_LT_GREY : VCOL_DARK_GREY;
	}

	// Start position of player
	sbyte	ix = px, iy = py;

	// Starting at first screen column
	sbyte	sx = 0;

	for(char i=0; i<7; i++)
	{
		// Next screen column
		sbyte	tx = 20 - zxdist[i];

		// View blocked by wall
		if (maze[iy][ix] == '#')
		{
			// Fill with wall color
			for(char cx=sx; cx<40-sx; cx++)
			{
				color_column(cx, cfront);
				screen_column(cx, sx, 96 + (sx & 3), 96, 96);
			}

			// And be done
			return ;
		}

		// Check for left wall
		if (maze[iy - dx][ix + dy] == '#')
		{
			// Draw left wall
			for(char cx=sx; cx<tx; cx++)
			{
				// Perspective
				sbyte	ty = cx / 4;
				sbyte	by = 25 - cx;

				color_column(cx, cleft);
				screen_column(cx, cx, 100 + (cx & 3), 96, 124);
			}
		}
		else
		{
			// Draw adjacent wall visible due to free space left
			sbyte	ty = tx / 4;
			sbyte	by = 25 - tx;

			// All at same height, wall is facing us
			for(char cx=sx; cx<tx; cx++)
			{
				color_column(cx, cfront);
				screen_column(cx, tx, 96 + (tx & 3), 96, 96);
			}
		}

		// Check for right wall
		if (maze[iy + dx][ix - dy] == '#')
		{
			for(char cx=sx; cx<tx; cx++)
			{
				sbyte	ty = cx / 4;
				sbyte	by = 25 - cx;
				color_column(39 - cx, cright);
				screen_column(39 - cx, cx, 107 - (cx & 3), 96, 125);
			}
		}
		else
		{
			sbyte	ty = tx / 4;
			sbyte	by = 25 - tx;
			for(char cx=sx; cx<tx; cx++)
			{
				color_column(39 - cx, cfront);
				screen_column(39 - cx, tx, 96 + (tx & 3), 96, 96);
			}
		}

		// Advance in maze
		sx = tx;
		ix += dx;
		iy += dy;		
	}
}

// Raster interrupts for ceiling and floor colors
RIRQCode	center, bottom;

int main(void)
{
	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);
	mmap_set(MMAP_NO_BASIC);

	// Switch screen
	vic_setmode(VICM_TEXT, Screen0, Font);

	// Change colors
	vic.color_border = VCOL_BLACK;

	// initialize raster IRQ
	rirq_init(true);

	// Build switch to scroll line IRQ
	rirq_build(&center, 1);
	// Change color for floor
	rirq_write(&center, 0, &vic.color_back, VCOL_BLACK);
	// Put it into the perspective focus point
	rirq_set(0, 50 + 2 * 20, &center);

	// Build the switch to normal IRQ
	rirq_build(&bottom, 1);
	// Change color for ceiling
	rirq_write(&bottom, 0, &vic.color_back, VCOL_WHITE);
	// place this at the bottom
	rirq_set(1, 250, &bottom);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// Block multiple rotations
	bool	rotate = false;	

	// Draw initial frame

	screen_flip();
	maze_draw(zxdist0);
	screen_flip();

	for(;;)
	{
		// Read joystick input
		joy_poll(0);

		// Forward or backward motion
		if (joyy[0])
		{
			// Target square
			sbyte tx = px - dx * joyy[0];
			sbyte ty = py - dy * joyy[0];

			// Check i empty
			if (maze[ty][tx] != '#')
			{
				if (joyy[0] < 0)
				{
					// Forward animation
					px = tx;
					py = ty;
					maze_draw(zxdist1);
					screen_flip();
				}
				else
				{
					// Backward animation
					maze_draw(zxdist1);
					screen_flip();
					px = tx;
					py = ty;					
				}
			}

			// New frame at new position
			maze_draw(zxdist0);
			screen_flip();			
		}

		// Check if new rotation
		if (!rotate)
		{
			if (joyx[0] == 1)
			{
				// Rotate right
				sbyte	t = dx; dx = -dy; dy = t;
				rotate = true;				
				maze_draw(zxdist0);
				screen_left();
			}
			else if (joyx[0] == -1)
			{
				// Rotate left
				sbyte	t = dx; dx = dy; dy = -t;
				rotate = true;
				maze_draw(zxdist0);
				screen_right();
			}
		}
		else if (!joyx[0])
		{
			// No rotation, may rotate again in next frame
			rotate = false;
		}
	}

	return 0;
}
