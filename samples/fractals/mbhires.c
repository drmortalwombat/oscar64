#include <string.h>
#include <c64/vic.h>
#include <c64/memmap.h>
#include <conio.h>

// Address of hires buffer and color buffer
#define Screen	((char *)0xe000)
#define Color	((char *)0xc800)

int main(void)
{
	// Install the IRQ trampoline
	mmap_trampoline();

	// Turn of the kernal ROM
	mmap_set(MMAP_NO_ROM);

	// Switch VIC into hires mode
	vic_setmode(VICM_HIRES, Color, Screen);

	// Clear the screen
	memset(Screen, 0, 8000);
	memset(Color, 0x10, 1000);

	// Loop over all pixels
	int	py, px;
	
	for(py=0; py<200; py++)
	{
		for(px=0; px<320; px++)
		{
			// Value in the complex plane

			float	xz = (float)px * (3.5 / 320.0)- 2.5;
			float	yz = (float)py * (2.0 / 200.0) - 1.0;
			
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
		
			// Set a pixel if exceeds bound in less than 32 iterations
			if (i < 32)
				Screen[320 * (py >> 3) + (py & 7) + (px & ~7)] |= 0x80 >> (px & 7);
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
