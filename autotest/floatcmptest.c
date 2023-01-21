#include <stdio.h>
#include <assert.h>


bool feq(float a, float b)
{
	return a == b;
}

bool flt(float a, float b)
{
	return a < b;
}

bool fle(float a, float b)
{
	return a <= b;
}

bool fgt(float a, float b)
{
	return a > b;
}

bool fge(float a, float b)
{
	return a >= b;
}


volatile float f;

inline void cmpflt(float a, float b, bool eq, bool lt, bool gt)
{
	bool	le = eq || lt;
	bool	ge = eq || gt;

	assert(feq(a, b) == eq);
	assert(flt(a, b) == lt);
	assert(fgt(a, b) == gt);
	assert(fle(a, b) == le);
	assert(fge(a, b) == ge);

	f = a;

	assert(feq(f, b) == eq);
	assert(flt(f, b) == lt);
	assert(fgt(f, b) == gt);
	assert(fle(f, b) == le);
	assert(fge(f, b) == ge);

	f = b;

	assert(feq(a, f) == eq);
	assert(flt(a, f) == lt);
	assert(fgt(a, f) == gt);
	assert(fle(a, f) == le);
	assert(fge(a, f) == ge);
}

int main(void)
{
	cmpflt( 0.0,  1.0, false, true, false);
	cmpflt( 0.0, -1.0, false, false, true);
	cmpflt( 1.0,  0.0, false, false, true);
	cmpflt(-1.0,  0.0, false, true, false);

#if 1
	cmpflt( 1.0,  1.0, true, false, false);
	cmpflt( 1.0,  2.0, false, true, false);
	cmpflt( 2.0,  1.0, false, false, true);

	cmpflt(-1.0, -1.0, true, false, false);
	cmpflt(-1.0, -2.0, false, false, true);
	cmpflt(-2.0, -1.0, false, true, false);
	
	cmpflt( 1.0, -1.0, false, false, true);
	cmpflt( 1.0, -2.0, false, false, true);
	cmpflt( 2.0, -1.0, false, false, true);

	cmpflt(-1.0,  1.0, false, true, false);
	cmpflt(-1.0,  2.0, false, true, false);
	cmpflt(-2.0,  1.0, false, true, false);

	cmpflt( 0.0,   0.0, true, false, false);
	cmpflt(-0.0,   0.0, true, false, false);
	cmpflt( 0.0,  -0.0, true, false, false);
	cmpflt(-0.0,  -0.0, true, false, false);

	cmpflt( 1.0,  1.000001,     false, true, false);
	cmpflt( 1.000001, 1.0,      false, false, true);
	cmpflt( 1.000001, 1.000001, true, false, false);

	cmpflt( -1.0,  -1.000001,     false, false, true);
	cmpflt( -1.000001, -1.0,      false, true, false);
	cmpflt( -1.000001, -1.000001, true, false, false);
#endif
	
	return 0;
}
