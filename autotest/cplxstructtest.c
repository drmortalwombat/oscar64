#include <stdio.h>
#include <math.h>
#include <assert.h>

struct cplx
{
	float	r, i;
};

cplx cplx_add(cplx a, cplx b)
{
	cplx	c;
	c.r = a.r + b.r;
	c.i = a.i + b.i;
	return c;
}

cplx cplx_sub(cplx a, cplx b)
{
	cplx	c;
	c.r = a.r - b.r;
	c.i = a.i - b.i;
	return c;	
}

cplx cplx_mul(cplx a, cplx b)
{
	cplx	c;
	c.r = a.r * b.r - a.i * b.i;
	c.i = a.i * b.r + a.r * b.i;
	return c;
}

float cplx_abs(cplx a)
{
	return sqrt(a.r * a.r + a.i * a.i);
}

cplx cplx_sum(cplx p, const cplx * a, int n)
{
	cplx	s	=	{0.0, 0.0};
	cplx	c	=	{1.0, 0.0};

	for(int i=0; i<n; i++)
	{
		s = cplx_add(s, cplx_mul(a[i], c));
		c = cplx_mul(c, p);
	}

	return s;
}

int main(void)
{
	cplx	sig[100];

	for(int i=0; i<100; i++)
	{
		sig[i].r = cos(i * 1.3);
		sig[i].i = sin(i * 1.3);
	}

	cplx	c	=	{1.0, 0.0};
	float	phi = 0.1 * PI;	
	cplx	t;
	t.r = cos(phi); t.i = sin(phi);

	float	p[20], q[20];

	for(int i=0; i<20; i++)
	{
		p[i] = cplx_abs(cplx_sum(c, sig, 100));
		c = cplx_mul(c, t);
	}

	for(int i=0; i<20; i++)
	{
		float	sumr = 0.0, sumi = 0.0;
		for(int j=0; j<100; j++)
		{
			float	co = cos(i * j * 0.1 * PI), si = sin(i * j * 0.1 * PI);
			sumr += co * sig[j].r - si * sig[j].i;
			sumi += si * sig[j].r + co * sig[j].i;
		}
		q[i] = sqrt(sumr * sumr + sumi * sumi);
	}
#if 1
	for(int i=0; i<20; i++)
	{
		printf("%d, %f - %f\n", i, p[i], q[i]);
	}
#endif
	for(int i=0; i<20; i++)
		assert(fabs(p[i] - q[i]) < 1.0);

	return 0;
}
