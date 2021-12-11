#include <math.h>
#include <stdio.h>
#include <assert.h>

int main(void)
{
	for(int i=0; i<1000; i++)
	{
		float	w = 0.0123 * i;
		float	co = cos(w), si = sin(w);

		float	r = co * co + si * si;

		assert(fabs(r - 1) < 0.001);

		assert(fabs(sqrt(w * w) - w) < 0.001);

		float	aw = atan2(si, co);

		float	co2 = cos(aw), si2 = sin(aw);

		assert(fabs(co2 - co) < 0.001);
		assert(fabs(si2 - si) < 0.001);

		float	ex = exp(w), em = exp(-w);

		assert(fabs(log(ex) - w) < 0.001);
		assert(fabs(log(em) + w) < 0.001);
	}

	return 0;
}
