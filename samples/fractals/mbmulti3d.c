#include <string.h>
#include <c64/vic.h>
#include <c64/memmap.h>
#include <conio.h>
#include <math.h>

// Address of hires buffer and color buffers
#define Screen	((char *)0xe000)
#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)

// Bit patterns for two different color pairs and eight shades
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

// Fill a vertical line x from py to ty with color c
void VLine(int x, int py, int ty, char c)
{
	// Clip boundaries
	if (py < 0)
		py = 0;
	if (ty > 100)
		ty = 100;

	// Check if there are pixel to draw
	if (py < ty)
	{
		// Calculate top address and mask
		char	mask = 0xc0 >> (2 * (x & 3));
		char	* dp = Screen + 320 * (py >> 2) + 2 * (py & 3) + 2 * (x & ~3);

		// Get the two color patterns
		char c0 = colors[0][c] & mask, c1 = colors[1][c] & mask;

		// Invert mask to cover the unchanged portion
		mask = ~mask;

		// Loop over all pixels
		char h = ty - py;
		while (h)
		{
			// Apply color to memory
			dp[0] = (dp[0] & mask) | c0; 
			dp[1] = (dp[1] & mask) | c1; 

			// Two pixel lines down
			dp += 2;
			if (!((int)dp & 7))
				dp += 312;

			h--;
		}
	}
}

// Iterate up to 32 iterations and return a smoothed height
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
		return i - log(log(r)/log(64.0))/log(2.0);
}

// Calculate light with given new and old heights
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
	// Install the IRQ trampoline
	mmap_trampoline();

	// Turn of the kernal ROM
	mmap_set(MMAP_NO_ROM);

	// Switch VIC into multicolor bitmap mode
	vic_setmode(VICM_HIRES_MC, Color1, Screen);

	// Clear the screen and set the colors
	vic.color_back = 0x00;
	vic.color_border = 0x00;

	memset(Screen, 0, 8000);
	memset(Color1, 0x26, 1000);
	memset(Color2, 0x0f, 1000);

	// Height of previous row, needed for lighting	
	float	hl[200];
	
	// Rotation of complex plane
	float	w = -0.7;
	float	co = cos(w), si = sin(w);
	
	// Loop from left to right
	for(int x=-1; x<160; x+= 1)
	{
		// Loop from far to nead
		int	py = 20;
		float	hu = 0;
		for(int y=1; y<200; y+= 1)
		{
			// Inverse 3D projection
			float fz = 2.0 / (float)y;
			float fx = (float)(x - 80) * fz / 100.0;
			
			float	mz = fz * 100.0 - 3.0, mx = fx * 100.0;

			// Rotation of the plane
			float	rx = mx * co - mz * si, rz = mx * si + mz * co;
			float	dp = iter(rx, rz);
			float	v = 2 * dp;
			if (v < 1.0) v = 1.0;
			
			float	fy = 5.0 * pow(2.0, - v * 0.4);
			
			// Calculate light
			int		ni = light(hl[y], hu, fy);

			// Update left column
			hl[y] = fy;
			hu = fy;

			// Forward 3D projection
			int		ty = 20 + y / 2 + (int)(floor(fy / fz));

			
			// color of pixel
			int		c;
			if (dp != 32) 
				c = 1 + ni + 8 * ((int)floor(dp) & 1);
			else
				c = 0;

			// Draw line if not dummy left row
			if (x >= 0)
				VLine(x, py, ty, c);

			py = ty;
		}
	}

	// Re-enable the kernal
	mmap_set(MMAP_NO_BASIC);

	// Wait for key press
	getch();	

	// Restore VIC state
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
	
	return 0;
}
