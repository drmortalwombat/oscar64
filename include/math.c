#include "math.h"
#include "stdio.h"

float cos(float f)
{
	return sin(f + 0.5 * PI);
}

#define	F_SIN_1	6.283168
#define	F_SIN_2	0.053878661
#define	F_SIN_3	-42.5385038
#define	F_SIN_4	9.23583223
#define	F_SIN_5	55.7310503


float sin(float f)
{
	float	g = fabs(f);
	float	m = f < 0.0 ? -1.0 : 1.0;
	
	g *= 0.5 / PI;
	g -= floor(g);

	if (g >= 0.5)
	{
		m = -m;
		g -= 0.5;
	}
	if (g >= 0.25)
		g = 0.5 - g;
	
	float	s = F_SIN_5;
	s *= g; s += F_SIN_4;
	s *= g; s += F_SIN_3;
	s *= g; s += F_SIN_2;
	s *= g; s += F_SIN_1;
	s *= g;
	
	return s * m;
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

#define F_ATAN_0 	-6.435678E-5
#define	F_ATAN_1	0.999648382
#define	F_ATAN_2	0.018278903
#define	F_ATAN_3	-0.444599298
#define	F_ATAN_4	0.263177486
#define	F_ATAN_5	-0.051078392


float atan2(float p, float q)
{
	int	quad = 0;
	
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
	
	float	s = F_ATAN_5;
	s *= g; s += F_ATAN_4;
	s *= g; s += F_ATAN_3;
	s *= g; s += F_ATAN_2;
	s *= g; s += F_ATAN_1;
	s *= g; s += F_ATAN_0;
	
	if (quad & 1)
		s = 0.5 * PI - s;
	if (quad & 2)
		s = PI - s;
	if (quad & 4)
		s = -s;
	
	return s;
}

#define F_EXP_0 	1.0000003
#define F_EXP_1 	0.693147059
#define F_EXP_2 	0.240173099
#define F_EXP_3 	0.055816392
#define F_EXP_4 	0.008965036
#define F_EXP_5 	0.001898429

float exp(float f)
{
	f *= 1.442695041;

	float	ff = floor(f), g = f - ff;
	
	int	fi = (int)ff;
	
	union {
		float	f;
		int		i[2];
	}	x;
	x.f = 0;

	x.i[1] = (fi + 0x7f) << 7;
	
	float	s = F_EXP_5;
	s *= g; s += F_EXP_4;
	s *= g; s += F_EXP_3;
	s *= g; s += F_EXP_2;
	s *= g; s += F_EXP_1;
	s *= g; s += F_EXP_0;

	return s * x.f;
}

#define F_LOG_0 	-2.79423993
#define F_LOG_1		5.05984692
#define F_LOG_2		-3.49247429
#define F_LOG_3		1.58181193
#define F_LOG_4		-0.396821126
#define F_LOG_5		0.041845518

float log(float f)
{
	if (f == 0.0)
		return 1.0;

	union {
		float	f;
		int		i[2];
	}	x;
	
	x.f = f;
	int	ei = x.i[1];
	int ex = (ei >> 7) - 0x7f;	
	x.i[1] = (ei & 0x007f) | 0x3f80;
	
	float	g = x.f;

	float	fex = ex;

	float	s = F_LOG_5;
	s *= g; s += F_LOG_4;
	s *= g; s += F_LOG_3;
	s *= g; s += F_LOG_2;
	s *= g; s += F_LOG_1;
	s *= g; s += F_LOG_0;
	
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
	if (f >= 0)
	{
		union {
			float	f;
			int		i[2];
		}	x;

		x.f = f;
		int ex = (x.i[1] >> 7) - 0x7f;
		ex /= 2;
		x.i[1] = (ex + 0x7f) << 7;
		float	fq = x.f;
		fq = 0.5 * (fq + f / fq);
		fq = 0.5 * (fq + f / fq);
		fq = 0.5 * (fq + f / fq);
		fq = 0.5 * (fq + f / fq);
		return fq;
	}
	else
		return 0.0;
}


bool isinf(float f)
{
	union {
		float		f;
		unsigned	i[2];
	}	x;

	x.f = f;

	return ((x.i[1] >> 7) & 0xff) == 0xff;
}

bool isfinite(float f)
{
	union {
		float		f;
		unsigned	i[2];
	}	x;

	x.f = f;

	return ((x.i[1] >> 7) & 0xff) != 0xff;
}
