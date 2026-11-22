#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int32_t castf(float x)
{
	union {float f; int32_t i;} t;
	t.f = x; 
	return t.i;
}

void dump(float a, float b, float c, float d, int limit = 16)
{
	printf("%g : [%08lx], %g : [%08lx], %g : [%08lx], %g : [%08lx]\n", a, a, b, b, c, c, d, d);
	assert(c == d || abs(castf(c) - castf(d)) <= limit);
}

__noinline void _mul(float a, float b, float c)
{
	float	d = a * b;
	dump(a, b, c, d);
}

__noinline void _add(float a, float b, float c)
{
	float	d = a + b;
	dump(a, b, c, d);
}

__noinline void _div(float a, float b, float c)
{
	float	d = a / b;
	dump(a, b, c, d);
}

__noinline void _divr(float a, float b, float c)
{
	float	d = a / b;
	dump(a, b, c, d, 64);
}

#define mul(a, b)	_mul(a, b, a * b)
#define add(a, b)	_add(a, b, a + b)
#define div(a, b)	_div(a, b, a / b)
#define divr(a, b)	_divr(a, b, a / b)

void test_add(void)
{
	add(1.0e-39, 1.0e-39);
	add(5.0e-39, 5.0e-39);
	add(5.5e-39, 6.0e-39);
	add(1.0e-38, -99.0e-40);
	add(1.0e-38, -999.0e-41);
	add(1.0e-38, -9999.0e-42);
	add(1.0e-40, -2.0e-40);

	add(1.0e38, 1.0e38);
	add(2.0e38, 2.0e38);

	add(1.0e38, -1.0e38);
	add(5.0e37, -1.0e38);

	add(INF, 1.0);
	add(1.0, INF);
}

void test_mul(void)
{
	mul(1.0, 1.0);
	mul(1.0, 0.0);
	mul(0.0, 1.0);

	mul(1.0e-18, 1.0e-18);
	mul(1.0e-19, 1.0e-19);
	mul(1.0e-20, 1.0e-20);
	mul(1.0e-24, 1.0e-24);

	mul(1.0e-37, 1.0e37);
	mul(1.0e-40, 1.0e38);

	mul(0.5e-37, 0.5);
	mul(0.5e-37, 0.25);
	mul(0.5e-37, 0.125);
	mul(0.5e-37, 0.06125);

	mul(1.0e-37, 1.0e-1);
	mul(1.0e-37, 1.0e-2);
	mul(1.0e-37, 1.0e-3);
	mul(1.0e-37, 1.0e-4);

	mul(1.0e-40, 1.0e-1);
	mul(1.0e-40, 1.0e-2);
	mul(1.0e-40, 1.0e-3);
	mul(1.0e-40, 1.0e-4);

	mul(1.0e19, 1.0e19);
	mul(2.0e19, 1.0e19);
	mul(2.0e19, 2.0e19);

	mul(INF, 0.1);
	mul(0.1, INF);
}


void test_div(void)
{
	div(1.0, 1.0);
	div(1.0, 0.0);
	div(0.0, 1.0);

	div(1.0e-18, 1.0e18);
	div(1.0e-19, 1.0e19);
	div(1.0e-20, 1.0e20);
	div(1.0e-24, 1.0e24);

	div(1.0e-37, 1.0e-37);
	div(1.0e-37, 1.0e-35);
	divr(1.0e-40, 1.0e-38);

	div(1.0e-37, 1.0e1);
	div(1.0e-37, 1.0e2);
	div(1.0e-37, 1.0e3);
	div(1.0e-37, 1.0e4);

	div(1.0e-40, 1.0e1);
	div(1.0e-40, 1.0e2);
	div(1.0e-40, 1.0e3);
	div(1.0e-40, 1.0e4);

	div(1.0e19, 1.0e-19);
	div(2.0e19, 1.0e-19);
	div(4.0e19, 1.0e-19);

	div(INF, 0.1);
	div(0.1, INF);
}

int main(void)
{
	test_add();
	test_mul();
	test_div();
}