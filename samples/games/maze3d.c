#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define Screen ((byte *)0x0400)
#define Color ((byte *)0xd800)


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
	"#.#..##....###.#",
	"#.##....#....#.#",
	"#.#####.######.#",
	"#..............#",
	"################",
};

void maze_init(void)
{
}


static char zxdist[] = {18, 6, 3, 2, 1, 0};

// Put one  char on screen
inline void screen_put(byte x, byte y, char ch, char color)
{
	__assume(y < 25);

	Screen[40 * y + x] = ch;
	Color[40 * y + x] = color;
}

// Get one char from screen
inline char screen_get(byte x, byte y)
{
	__assume(y < 25);

	return Screen[40 * y + x];
}

sbyte	px = 1, py = 3, dx = 1, dy = 0;

void maze_draw(void)
{
	sbyte	ix = px, iy = py;
	sbyte	sx = 0;
	for(char i=0; i<6; i++)
	{
		sbyte	tx = 20 - zxdist[i];

		sbyte	ty = sx / 4;
		sbyte	by = 25 - sx;

		if (maze[iy][ix] == '#')
		{
			for(char cy=0; cy<ty; cy++)
			{
				for(char cx=sx; cx<20; cx++)
				{
					screen_put(     cx, cy, 32, 0);				
					screen_put(39 - cx, cy, 32, 0);			
				}
			}
			for(char cy=ty; cy<by; cy++)
			{
				for(char cx=sx; cx<20; cx++)
				{
					screen_put(     cx, cy, 102, 0);				
					screen_put(39 - cx, cy, 102, 0);			
				}
			}
			for(char cy=by; cy<25; cy++)
			{
				for(char cx=sx; cx<20; cx++)
				{
					screen_put(     cx, cy, 32, 0);				
					screen_put(39 - cx, cy, 32, 0);			
				}
			}
			return ;
		}

		if (maze[iy - dx][ix + dy] == '#')
		{
			for(char x=sx; x<tx; x++)
			{
				sbyte	ty = x / 4;
				sbyte	by = 25 - x;
				for(char cy=0; cy<ty; cy++)
					screen_put(x, cy, 32, 0);
				for(char cy=ty; cy<by; cy++)
					screen_put(x, cy, 160, 0);
				for(char cy=by; cy<25; cy++)
					screen_put(x, cy, 32, 0);
			}
		}
		else
		{
			sbyte	ty = tx / 4;
			sbyte	by = 25 - tx;
			for(char x=sx; x<tx; x++)
			{
				for(char cy=0; cy<ty; cy++)
					screen_put(x, cy, 32, 0);
				for(char cy=ty; cy<by; cy++)
					screen_put(x, cy, 102, 0);
				for(char cy=by; cy<25; cy++)
					screen_put(x, cy, 32, 0);
			}
		}

		if (maze[iy + dx][ix - dy] == '#')
		{
			for(char x=sx; x<tx; x++)
			{
				sbyte	ty = x / 4;
				sbyte	by = 25 - x;
				for(char cy=0; cy<ty; cy++)
					screen_put(39 - x, cy, 32, 0);
				for(char cy=ty; cy<by; cy++)
					screen_put(39 - x, cy, 160, 0);
				for(char cy=by; cy<25; cy++)
					screen_put(39 - x, cy, 32, 0);
			}
		}
		else
		{
			sbyte	ty = tx / 4;
			sbyte	by = 25 - tx;
			for(char x=sx; x<tx; x++)
			{
				for(char cy=0; cy<ty; cy++)
					screen_put(39 - x, cy, 32, 0);
				for(char cy=ty; cy<by; cy++)
					screen_put(39 - x, cy, 102, 0);
				for(char cy=by; cy<25; cy++)
					screen_put(39 - x, cy, 32, 0);
			}
		}

		sx = tx;
		ix += dx;
		iy += dy;		
	}
}

int main(void)
{
#if 0
	for(char i=0; i<8; i++)
	{
		float	z = 0.5 + i;
		float	x = 9.0 / z;
		printf("%d : %f / %f : %d\n", i, z, x, (int)x);
	}
	return 0;
#endif
	bool	rotate = false;

	for(;;)
	{
		maze_draw();
		joy_poll(1);

		sbyte tx = px - dx * joyy[1];
		sbyte ty = py - dy * joyy[1];

		if (maze[ty][tx] != '#')
		{
			px = tx;
			py = ty;
		}

		if (!rotate)
		{
			if (joyx[1] == 1)
			{
				sbyte	t = dx; dx = - dy; dy = t;
				rotate = true;
			}
			else if (joyx[1] == -1)
			{
				sbyte	t = dx; dx = dy; dy = -t;
				rotate = true;
			}
		}
		else if (!joyx[1])
		{
			rotate = false;
		}
	}

	return 0;
}
