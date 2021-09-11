#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int main(void)
{
	printf("%f\n", 0.0);
	

	float	x = 1.0, y = 1.0;
	
	char	xb[20], yb[20];
	for(int i=0; i<40; i++)
	{
		ftoa(x, xb); float xr = atof(xb);
		ftoa(y, yb); float yr = atof(yb);
		
		printf("%20g (%s) %20g : %20g (%s) %20g : %10f %10f \n", x, xb, xr, y, yb, y, fabs(x - xr) / x, fabs(y - yr) / y);
		
		if (fabs(x - xr) / x > 0.00001 || fabs(y - yr) / y > 0.00001)
			return -1;

		x *= 2.5;
		y *= 0.8;
	}
	
	return 0;
}




