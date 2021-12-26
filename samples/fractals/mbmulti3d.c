#include <string.h>
#include <c64/vic.h>
#include <c64/memmap.h>
#include <conio.h>
#include <math.h>

#define Screen	((char *)0xe000)
#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)

byte	colors[2][17] = 
{
	{0x00, 
	 0x44, 0x44, 0x55, 0x55, 0xdd, 0xdd, 0xff, 0xff,
	 0x88, 0x88, 0xaa, 0xaa, 0xee, 0xee, 0xff, 0xff,
	},
	{0x00, 
	 0x00, 0x11, 0x11, 0x55, 0x55, 0x77, 0x77, 0xff,
	 0x00, 0x22, 0x22, 0xaa, 0xaa, 0xbb, 0xbb, 0xff,
	}
};

void VLine(int x, int py, int ty, char c)
{
	if (py < 0)
		py = 0;
	if (ty > 100)
		ty = 100;

	if (py < ty)
	{
		char	mask = 0xc0 >> (2 * (x & 3));
		char	* dp = Screen + 320 * (py >> 2) + 2 * (py & 3) + 2 * (x & ~3);


		char c0 = colors[0][c] & mask, c1 = colors[1][c] & mask;
		mask = ~mask;
		char h = ty - py;
		while (h)
		{
			dp[0] = (dp[0] & mask) | c0; 
			dp[1] = (dp[1] & mask) | c1; 

			dp += 2;
			if (!((int)dp & 7))
				dp += 312;

			h--;
		}
	}
}

float iter(float xz, float yz)
{
	float	x = 0.0, y = 0.0, r;
	int		i;
	for(i=0; i<32; i++)
	{
		r = x * x + y * y;
		if (r > 64.0) break;
				
		float	xt = x * x - y * y + xz;
		y = 2 * x * y + yz;
		x = xt;
	}
	
	
	if (i == 32)
		return 32;
	else
		return i - log(log(r)/log(64.0))/log(2.0)
}

int light(float hl, float hu, float h)
{
	float	dx = h - hl, dz = h - hu, dy = 0.1;
	
	float	dd = sqrt(dx * dx + dy * dy + dz * dz);
	int		ni = (int)floor((-2 * dx + dy + dz) / dd * 0.408 * 8);
	if (ni < 0) ni = 0; else if (ni > 7) ni = 7;

	return ni;
}

int main(void)
{
	mmap_trampoline();

	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES_MC, Color1, Screen);

	vic.color_back = 0x00;
	vic.color_border = 0x00;

	memset(Screen, 0, 8000);
	memset(Color1, 0x26, 1000);
	memset(Color2, 0x0f, 1000);
	
	float	hl[200];
	
	float	w = -0.7;
	float	co = cos(w), si = sin(w);
	
	for(int x=-1; x<160; x+= 1)
	{
		int	py = 20;
		float	hu = 0;
		for(int y=1; y<200; y+= 1)
		{
			float fz = 2.0 / (float)y;
			float fx = (float)(x - 80) * fz / 100.0;
			
			float	mz = fz * 100.0 - 3.0, mx = fx * 100.0;

			float	rx = mx * co - mz * si, rz = mx * si + mz * co;
			float	dp = iter(rx, rz);
			float	v = 2 * dp;
			if (v < 1.0) v = 1.0;
			
			float	fy = 5.0 * pow(2.0, - v * 0.4);
			
			int		ni = light(hl[y], hu, fy);

			hl[y] = fy;
			hu = fy;

			int		ty = 20 + y / 2 + (int)(floor(fy / fz));

			int		c;
			
			if (dp != 32) 
				c = 1 + ni + 8 * ((int)floor(dp) & 1);
			else
				c = 0;

			if (x >= 0)
				VLine(x, py, ty, c);

			py = ty;
		}
	}

	mmap_set(MMAP_NO_BASIC);

	getch();	

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
	
	return 0;
}
