#include <string.h>
#include <c64/vic.h>
#include <c64/memmap.h>
#include <conio.h>

// Address of hires buffer and color buffers
#define Screen	((char *)0xe000)
#define Color	((char *)0xc800)
#define Color2	((char *)0xd800)

// Bit patterns for eight different color pairs
char colors[] = {
	0xff, 0xff, 
	0xee, 0xbb,
	0xaa, 0xaa,
	0x88, 0x22,

	0x44, 0x11,
	0x55, 0x55,
	0xdd, 0x77,
	0x33, 0xcc
};

int main(void)
{
	// Install the IRQ trampoline
	mmap_trampoline();

	// Turn of the kernal ROM
	mmap_set(MMAP_NO_ROM);

	// Switch VIC into multicolor bitmap mode
	vic_setmode(VICM_HIRES_MC, Color, Screen);

	// Clear the screen and set the colors
	vic.color_back = 0x00;

	memset(Screen, 0, 8000);
	memset(Color, 0x27, 1000);
	memset(Color2, 0x03, 1000);

	// Loop over all pixels
	int	py, px;
	
	for(py=0; py<100; py++)
	{
		for(px=0; px<160; px++)
		{
			// Value in the complex plane

			float	xz = (float)px * (3.5 / 160.0)- 2.5;
			float	yz = (float)py * (2.4 / 100.0) - 1.2;
			
			// Iterate up to 32 times
			float	x = 0.0, y = 0.0;
			int		i;
			for(i=0; i<32; i++)
			{
				if (x * x + y * y > 4.0) break;
						
				float	xt = x * x - y * y + xz;
				y = 2 * x * y + yz;
				x = xt;
			}
		
			if (i < 32)
			{
				// Position on screen
				char	* dp = Screen + 320 * (py >> 2) + 2 * (py & 3) + 2 * (px & ~3);

				// Mask of pixels to change
				char	mask = 0xc0 >> (2 * (px & 3));

				// Get the two color patterns for upper and lower half
				char	c0 = colors[2 * (i & 7)], c1 = colors[2 * (i & 7) + 1];

				// Put the pixels into the image
				dp[0] |= c0 & mask;
				dp[1] |= c1 & mask;
			}
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
