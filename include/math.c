#include "math.h"
#include "stdio.h"

static __noinline float poly_mul(const float * fp, float g)
{
	float s = fp[0];
	for(int i=1; i<6; i++)
	{
		s *= g;
		s += fp[i];
	}
	return s;
}


float cos(float f)
{
	return sin(f + 0.5 * PI);
}

static const float sin_poly[] = {
	-15.09466089,
	42.05873568,
	-76.70591896,
	81.60529427,	
	-41.34171592,
	6.283186
};

float sin(float f)
{
	float	g = f * (0.5 / PI);
	
	g -= floor(g);

	if (g >= 0.5)
		g -= 1.0;

	if (g >= 0.25)
		g = 0.5 - g;
	else if (g <= -0.25)
		g = -0.5 - g;

	return poly_mul(sin_poly, g * g) * g;
}

float tan(float f)
{
	return sin(f) / cos(f);
}

float acos(float f)
{
	return atan2(sqrt(1.0 - f * f), f);
}

float asin(float f)
{
	return atan2(f, sqrt(1.0 - f * f));
}

float atan(float f)
{
	return atan2(f, 1.0);
}

static const float atan_poly[] = {
		-0.011535025,
		0.052216655,
		-0.116062757,
		0.193405977,
		-0.332602256,
		0.999976302
};

float atan2(float p, float q)
{
	char	quad = 0;
	
	if (p < 0)
	{
		quad |= 4;
		p = -p;
	}
	if (q < 0)
	{
		quad |= 2;
		q = -q;
	}
	
	float	g;
	if (p > q)
	{
		g = q / p;
		quad |= 1;
	}
	else
	{
		g = p / q;
	}
	
	float s = poly_mul(atan_poly, g * g) * g;

	if (quad & 1)
		s = 0.5 * PI - s;
	if (quad & 2)
		s = PI - s;
	if (quad & 4)
		s = -s;
	
	return s;
}

static const float	exp_poly[] = {
	0.001894414,
	0.008940801,
	0.055876024,
	0.240132068,
	0.69315668,
	1.0
};

float exp(float f)
{
	f *= 1.442695041;

	float	ff = floor(f), g = f - ff;
	
	int	fi = (int)ff;

	float xf = castu2f((unsigned long)(fi + 0x7f) << 23);

	float s = poly_mul(exp_poly, g);

	return s * xf;
}

static const float	log_poly[] = {
	1.41160635,
	-6.58255755,
	12.9408785,
	-14.1347166,	
	10.1628473,
	-3.79805942	
};

float log(float f)
{
	if (f <= 0)
		return -INF;	

	unsigned long uf = castf2u(f);
	int ex = (int)(uf >> 23) - 0x7e;
	float g = castu2f(uf & 0x007ffffful | 0x3f000000ul);

	float	fex = ex;

	float s = poly_mul(log_poly, g);

	return (fex + s) * 0.6931471806;
}

float log10(float f)
{
	return log(f) * 0.4342944819;
}

float pow(float p, float q)
{
	return exp(q * log(p));
}

float sqrt(float f)
{
	if (f > 0)
	{
		float fq = castu2f(((castf2u(f) & 0x7f800000ul) + 0x3f800000ul) >> 1);

		for(int i=0; i<4; i++)
			fq = 0.5 * (fq + f / fq);

		return fq;
	}
	else if (f == 0.0)
		return 0.0;
	else
		return -INF;
}


bool isinf(float f)
{
	return ((castf2u(f) >> 23) & 0xff) == 0xff;
}

bool isfinite(float f)
{
	return ((castf2u(f) >> 23) & 0xff) != 0xff;
}
