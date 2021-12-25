#include <string.h>

#define Screen	((char *)0x0400)
#define Color	((char *)0xd800)

int main(void)
{
	memset(Screen, 160, 1024);

	int	py, px;
	
	for(py=0; py<25; py++)
	{
		for(px=0; px<40; px++)
		{
			float	xz = (float)px * (3.5 / 40.0)- 2.5;
			float	yz = (float)py * (2.0 / 25.0) - 1.0;
			
			float	x = 0.0, y = 0.0;
			int		i;
			for(i=0; i<=14; i++)
			{
				if (x * x + y * y > 4.0) break;
						
				float	xt = x * x - y * y + xz;
				y = 2 * x * y + yz;
				x = xt;
			}
			i--;
		
			Color[py * 40 + px] = i;
		}
	}
	
	return 0;
}
