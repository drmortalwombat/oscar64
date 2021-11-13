#include <assert.h>

void testint0(void)
{
	int	a0n = 0, an0 = 0;
	int	b0n = 0, bn0 = 0;

	for(int i=-1000; i<1000; i++)
	{
		if (i >= 0 && i < 500)
			a0n += 1;
		if (i < 500 && i >= 0)
			an0 += 1;
		if (i >= 0 && i <= 499)
			b0n += 1;
		if (i <= 499 && i >= 0)
			bn0 += 1;
	}

	assert(a0n == 500);
	assert(an0 == 500);
	assert(b0n == 500);
	assert(bn0 == 500);	
}

typedef signed char 	int8;

void testbyte0(void)
{
	int8	a0n = 0, an0 = 0;
	int8	b0n = 0, bn0 = 0;

	for(int8 i=-100; i<100; i++)
	{
		if (i >= 0 && i < 50)
			a0n += 1;
		if (i < 50 && i >= 0)
			an0 += 1;
		if (i >= 0 && i <= 49)
			b0n += 1;
		if (i <= 49 && i >= 0)
			bn0 += 1;
	}

	assert(a0n == 50);
	assert(an0 == 50);
	assert(b0n == 50);
	assert(bn0 == 50);	
}

int main(void)
{
	testint0();
	testbyte0();

	return 0;
}

