#include <assert.h>
#include <stdio.h>
#include <math.h>


int main(void)
{
	float			a;
	int 			i;
	long			li;
	unsigned		u;
	unsigned long	lu;

	a = 1.0;
	i = 1;	
	for(int j=0; j<15; j++)
	{
		assert(i == (int)a);
		assert(a == (float)i);
		a *= 2.0;
		i <<= 1;
	}

	a = -1.0;
	i = -1;	
	for(int j=0; j<15; j++)
	{
		assert(i == (int)a);
		assert(a == (float)i);
		a *= 2.0;
		i <<= 1;
	}

	a = 1.0;
	i = 1;	
	for(int j=0; j<15; j++)
	{
		assert(i == (int)a);
		assert(a == (float)i);
		a *= 2.0;
		a += 1.0;
		i <<= 1;
		i += 1;
	}


	a = 1.0;
	u = 1;	
	for(int j=0; j<16; j++)
	{
		assert(u == (unsigned)a);
		assert(a == (float)u);
		a *= 2.0;
		u <<= 1;
	}

	a = 1.0;
	u = 1;	
	for(int j=0; j<16; j++)
	{
		assert(u == (unsigned)a);
		assert(a == (float)u);
		a *= 2.0;
		a += 1;
		u <<= 1;
		u += 1;
	}

	a = 1.0;
	li = 1;	
	for(int j=0; j<31; j++)
	{
		assert(li == (long)a);
		assert(a == (float)li);
		a *= 2.0;
		li <<= 1;
	}

	a = -1.0;
	li = -1;	
	for(int j=0; j<31; j++)
	{
		assert(li == (long)a);
		assert(a == (float)li);
		a *= 2.0;
		li <<= 1;
	}

	a = 1.0;
	lu = 1;	
	for(int j=0; j<32; j++)
	{
		assert(lu == (unsigned long)a);
		assert(a == (float)lu);
		a *= 2.0;
		lu <<= 1;
	}

	return 0;
}
