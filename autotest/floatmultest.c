#include <assert.h>
#include <stdio.h>
#include <math.h>

float	c = 1.25;
float	d = 1.0001;

int main(void)
{
	int		i;
	float	a = 0.0;
	
	for(i=0; i<50; i++)
	{		
//		printf("%d %f %f %f\n", i, i * c, a, i * c - a);
		assert(i * c == a);
		a += c;
	}

	a = d;
	
	for(i=1; i<50; i++)
	{		
//		printf("%d %f %f %f\n", i, i * d, a, fabs(i * d - a) / i);
		assert(fabs(i * d - a) < i * 1.0e-6);
		a += d;
	}
	
	return 0;
}
