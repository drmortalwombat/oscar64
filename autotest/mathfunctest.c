#include <stdio.h>
#include <math.h>
#include <assert.h>

__noinline void test_log(float f, float l)
{
	float v = log(f);
	printf("log(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define LOG(f) test_log(f, log(f))

__noinline void test_exp(float f, float l)
{
	float v = exp(f);
	printf("exp(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define EXP(f) test_exp(f, exp(f))

__noinline void test_sin(float f, float l)
{
	float v = sin(f);
	printf("sin(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define SIN(f) test_sin(f, sin(f))

__noinline void test_atan(float f, float l)
{
	float v = atan(f);
	printf("atan(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define ATAN(f) test_atan(f, atan(f))

__noinline void test_sqrt(float f, float l)
{
	float v = sqrt(f);
	printf("sqrt(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define SQRT(f) test_sqrt(f, sqrt(f))

__noinline void test_floor(float f, float l)
{
	float v = floor(f);
	printf("floor(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define FLOOR(f) test_floor(f, floor(f))

__noinline void test_ceil(float f, float l)
{
	float v = ceil(f);
	printf("ceil(%f) : %f -> %f (%g)\n", f, l, v, l - v);
	assert(isinf(l) && isinf(v) || fabs(l - v) < 0.0001);
}

#define CEIL(f) test_ceil(f, ceil(f))

int main(void)
{

	LOG(-1.0);
	LOG( 0.0);
	LOG( 0.5);
	LOG( 1.0);
	LOG( 2.0);
	LOG( 2.1);
	LOG( 3.0);
	LOG( 3.9);
	LOG( 4.0);

	EXP(-2.0);
	EXP(-1.0);
	EXP( 0.0);
	EXP( 0.5);
	EXP( 1.0);
	EXP( 2.0);
	EXP( 3.0);
	EXP( 3.9);

	SIN(-10);
	SIN(-PI);
	SIN(-0.5 * PI);
	SIN(-0.25 * PI);
	SIN(-0.01);
	SIN(0.0);
	SIN(0.01);
	SIN(0.25 * PI);
	SIN(0.5 * PI - 0.01);
	SIN(0.5 * PI);
	SIN(0.5 * PI + 0.01);
	SIN(PI - 0.01);
	SIN(PI);
	SIN(PI + 0.01);
	SIN(10);

	ATAN(-10);
	ATAN(-2);
	ATAN(-1.001);
	ATAN(-1);
	ATAN(-0.999);
	ATAN(-0.01);
	ATAN( 0);
	ATAN( 0.01);
	ATAN( 0.999);
	ATAN( 1);
	ATAN( 1.001);
	ATAN( 2);
	ATAN(10);

	SQRT(-1);
	SQRT( 0);
	SQRT( 0.0001);
	SQRT( 0.01);
	SQRT( 0.999);
	SQRT( 1);
	SQRT( 1.001);
	SQRT( 2);
	SQRT( 3);
	SQRT( 4);
	SQRT( 5);
	SQRT( 6);
	SQRT( 7);
	SQRT( 8);
	SQRT( 9);
	SQRT(10);
	SQRT(100);
	SQRT(10000);

	FLOOR(INF);
	FLOOR(-10.1);
	FLOOR(-10);
	FLOOR(-1);
	FLOOR(-0.5);
	FLOOR(-0.0001);
	FLOOR( 0);
	FLOOR(0.00001);
	FLOOR(0.5);
	FLOOR( 1);
	FLOOR(  9.9999);
	FLOOR( 10);
	FLOOR( 10.0001);
	FLOOR(1.0e-40);

	FLOOR(1.0e-10);
	FLOOR(1.0e-9);
	FLOOR(1.0e-8);
	FLOOR(1.0e-7);
	FLOOR(1.0e-6);
	FLOOR(1.0e-5);
	FLOOR(1.0e-4);
	FLOOR(1.0e-3);
	FLOOR(1.0e-2);
	FLOOR(1.0e-1);

	FLOOR(1.1);
	FLOOR(10.1);
	FLOOR(100.1);
	FLOOR(1000.1);
	FLOOR(10000.1);
	FLOOR(100000.1);
	FLOOR(1000000.1);

	CEIL(INF);
	CEIL(-10.1);
	CEIL(-10);
	CEIL(-1);
	CEIL(-0.5);
	CEIL(-0.0001);
	CEIL( 0);
	CEIL(0.00001);
	CEIL(0.5);
	CEIL( 1);
	CEIL(  9.9999);
	CEIL( 10);
	CEIL( 10.0001);
	CEIL(1.0e-40);

	CEIL(1.0e-10);
	CEIL(1.0e-9);
	CEIL(1.0e-8);
	CEIL(1.0e-7);
	CEIL(1.0e-6);
	CEIL(1.0e-5);
	CEIL(1.0e-4);
	CEIL(1.0e-3);
	CEIL(1.0e-2);
	CEIL(1.0e-1);

	CEIL(1.1);
	CEIL(10.1);
	CEIL(100.1);
	CEIL(1000.1);
	CEIL(10000.1);
	CEIL(100000.1);
	CEIL(1000000.1);
}