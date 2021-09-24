#include <stdio.h>
#include <math.h>

int main(void)
{
	for(int y=1; y<100; y++)
	{
		float fz = 100.0 / (float)y;
		printf("%d %f %f\n", y, floor(fz * 100.0), fz * 100.0);
	}
	
	return 0;	
}
