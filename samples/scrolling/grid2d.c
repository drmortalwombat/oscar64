#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdlib.h>
#include <c64/rasterirq.h>

#define screen ((byte *)0x0400)
#define color ((byte *)0xd800)
#define sline(x, y) (screen + 40 * (y) + (x))

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

void expandrow0(char * dp, const char * grid, char ly)
{
	char gi;
#assign gx 0		
#repeat
	gi = grid[gx] | ly;
	dp[4 * gx + 0] = quad[0][gi];
	dp[4 * gx + 1] = quad[1][gi];
	dp[4 * gx + 2] = quad[2][gi];
	dp[4 * gx + 3] = quad[3][gi];
#assign gx gx + 1
#until gx == 10
}

void expandrow1(char * dp, const char * grid, char ly)
{
	char gi;
	gi = grid[0] | ly;
	dp[0] = quad[1][gi];
	dp[1] = quad[2][gi];
	dp[2] = quad[3][gi];
#assign gx 0
#repeat
	gi = grid[gx + 1] | ly;
	dp[4 * gx + 3] = quad[0][gi];
	dp[4 * gx + 4] = quad[1][gi];
	dp[4 * gx + 5] = quad[2][gi];
	dp[4 * gx + 6] = quad[3][gi];
#assign gx gx + 1
#until gx == 9
	gi = grid[10] | ly;
	dp[39] = quad[0][gi];
}

void expandrow2(char * dp, const char * grid, char ly)
{
	char gi;
	gi = grid[0] | ly;
	dp[0] = quad[2][gi];
	dp[1] = quad[3][gi];
#assign gx 0
#repeat
	gi = grid[gx + 1] | ly;
	dp[4 * gx + 2] = quad[0][gi];
	dp[4 * gx + 3] = quad[1][gi];
	dp[4 * gx + 4] = quad[2][gi];
	dp[4 * gx + 5] = quad[3][gi];
#assign gx gx + 1
#until gx == 9
	gi = grid[10] | ly;
	dp[38] = quad[0][gi];
	dp[39] = quad[1][gi];
}

void expandrow3(char * dp, const char * grid, char ly)
{
	char gi;
	gi = grid[0] | ly;
	dp[0] = quad[3][gi];
#assign gx 0
#repeat
	gi = grid[gx + 1] | ly;
	dp[4 * gx + 1] = quad[0][gi];
	dp[4 * gx + 2] = quad[1][gi];
	dp[4 * gx + 3] = quad[2][gi];
	dp[4 * gx + 4] = quad[3][gi];
#assign gx gx + 1
#until gx == 9
	gi = grid[10] | ly;
	dp[37] = quad[0][gi];
	dp[38] = quad[1][gi];
	dp[39] = quad[2][gi];
}

void expand(char * dp, const char * grid, char px, char py)
{
	char ry = 4 * (py & 3);
	char rx = px & 3;

	char * cdp = dp;
	const char * cgrid = grid + (px >> 2) + 16 * (py >> 2);

	for(char gy=0; gy<20; gy++)
	{
		switch (rx)
		{
		case 0:
			expandrow0(cdp, cgrid, ry);
			break;
		case 1:
			expandrow1(cdp, cgrid, ry);
			break;
		case 2:
			expandrow2(cdp, cgrid, ry);
			break;
		default:
			expandrow3(cdp, cgrid, ry);
			break;
		}
		cdp += 40;
		ry += 4;
		if (ry == 16)
		{
			ry = 0;
			cgrid += 16;
		}
	}
}


char grid[16][16];

#pragma align(grid, 256)

RIRQCode	blank, scroll, bottom;

int main(void)
{
	for(char y=0; y<16; y++)
	{
		for(char x=0; x<16; x++)
		{
			grid[y][x] = rand() & 3;
		}
	}

	vic.color_border = 0;

	rirq_init(true);

	rirq_build(&blank, 1);
	rirq_write(&blank, 0, &vic.ctrl1, 0);
	rirq_set(0, 46 + 5 * 8, &blank);

	rirq_build(&scroll, 3);
	rirq_delay(&scroll, 10);
	rirq_write(&scroll, 1, &vic.ctrl1, VIC_CTRL1_DEN);
	rirq_write(&scroll, 2, &vic.ctrl2, 0);
	rirq_set(1, 54 + 5 * 8, &scroll);

	rirq_build(&bottom, 2);
	rirq_write(&bottom, 0, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL);
	rirq_write(&bottom, 1, &vic.ctrl2, VIC_CTRL2_CSEL);
	rirq_set(2, 250, &bottom);

	rirq_sort();

	rirq_start();

	int py = 40 * 32, px = 40 * 32, dy = 0, dx = 0, ax = 0, ay = 0;
	for(;;)
	{	
		int rx = px >> 5, ry = py >> 5;

		vic.color_border++;
		rirq_wait();
		vic.color_border--;

		rirq_data(&blank, 0, ((7 - ry) & 7) | VIC_CTRL1_DEN | VIC_CTRL1_BMM | VIC_CTRL1_ECM);		
		if ((ry & 7) == 0)
			rirq_data(&scroll, 0,  4);
		else
			rirq_data(&scroll, 0, 10);
		rirq_data(&scroll, 1, ((7 - ry) & 7) | VIC_CTRL1_DEN);
		rirq_data(&scroll, 2, (7 - rx) & 7);

		expand(screen + 200, &(grid[0][0]), rx >> 3, ry >> 3);

		dx += ax;
		dy += ay;

		if ((rand() & 127) == 0)
		{
			ax = (rand() & 63) - 32;
			ay = (rand() & 63) - 32;
		}

		dx -= (dx + 8) >> 4;
		dy -= (dy + 8) >> 4;

		py += dy;
		if (py < 0 || py > 10 * 8 * 4 * 32)
		{
			dy = -dy;
			py += dy;
		}

		px += dx;
		if (px < 0 || px > 6 * 8 * 4 * 32)
		{
			dx = -dx;
			px += dx;
		}
	}


	return 0;
}
