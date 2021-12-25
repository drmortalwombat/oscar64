#include <string.h>
#include <c64/vic.h>
#include <c64/memmap.h>

#define Screen	((char *)0xe000)
#define Color	((char *)0xc800)

int main(void)
{
	mmap_trampoline();

	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES, Color, Screen);

	memset(Screen, 0, 8000);
	memset(Color, 0x10, 1000);

	int	py, px;
	
	for(py=0; py<200; py++)
	{
		for(px=0; px<320; px++)
		{
			float	xz = (float)px * (3.5 / 320.0)- 2.5;
			float	yz = (float)py * (2.0 / 200.0) - 1.0;
			
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
				Screen[320 * (py >> 3) + (py & 7) + (px & ~7)] |= 0x80 >> (px & 7);
		}
	}
	
	return 0;
}
