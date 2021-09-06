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

bool fgt(float a, float b)
{
	return a > b;
}

void cmpflt(float a, float b, bool eq, bool lt, bool gt)
{
	assert(feq(a, b) == eq);
	assert(flt(a, b) == lt);
	assert(fgt(a, b) == gt);
}

int main(void)
{
	cmpflt( 0.0,  1.0, false, true, false);
	cmpflt( 0.0, -1.0, false, false, true);
	cmpflt( 1.0,  0.0, false, false, true);
	cmpflt(-1.0,  0.0, false, true, false);


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
	
	return 0;
}
