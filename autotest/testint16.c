#include <assert.h>

void testmuli(int a, int b, int ab)
{
	assert (a * b == ab);
}

void testdivi(int a, int b, int ab)
{
	assert (a / b == ab);
}

void shltesti(int a, int b, int ab)
{
	assert (a << b == ab);
}

void shrtesti(int a, int b, int ab)
{
	assert (a >> b == ab);
}

int sieve(int size)
{
	bool	sieve[1000];
	
	for(int i=0; i<size; i+=2)
	{
		sieve[i] = false;
		sieve[i+1] = true;
	}
	sieve[2] = true;
	
	for (int i = 3; i * i < size;)
	{
		int j = i * i;
		while (j < size)
		{
			sieve[j] = false;
			j += 2 * i;
		}
		do {i++;} while (i < size && !sieve[i]);
	}	
	
	int	num = 0;
	for(int i=0; i<size; i++)
	{
		if (sieve[i])
			num++;			
	}
	
	return num;
}

int main(void)
{	
	testmuli(0, 0, 0);
	testmuli(1, 0, 0);
	testmuli(0, 1, 0);
	
	testmuli( 1,  1,  1);
	testmuli(-1,  1, -1);
	testmuli(-1, -1,  1);
	testmuli( 1, -1, -1);
	
	testmuli(5, 5, 25);
	testmuli( 127,  255, 32385);
	testmuli(-127,  255, -32385);
	testmuli( 127, -255, -32385);
	testmuli(-127, -255, 32385);

	testdivi( 1,  1,  1);
	testdivi(-1,  1, -1);
	testdivi( 1, -1, -1);
	testdivi(-1, -1,  1);
	
	testdivi( 11,  4,  2);
	testdivi(-11,  4, -2);
	testdivi( 11, -4, -2);
	testdivi(-11, -4,  2);
	
	shltesti( 17, 1, 34);
	shltesti(-17, 1, -34);
	shltesti( 1700, 1, 3400);
	shltesti(-1700, 1, -3400);

	shrtesti( 34, 1, 17);
	shrtesti(-34, 1, -17);
	shrtesti( 3400, 1, 1700);
	shrtesti(-3400, 1, -1700);

	shrtesti( -1, 15, -1);
	shrtesti(32767, 15, 0);
	shrtesti( -1, 14, -1);
	shrtesti(32767, 14, 1);

	shltesti( -1, 14, -16384);
	shltesti(  1, 14,  16384);
	
	assert(sieve(200) == 47);
	assert(sieve(1000) == 169);
	
	int	a = 0, b = 0;
	for(int i=0; i<1000; i++)
	{
		assert( 17 * i == a);
		assert(-17 * i == b);
		a += 17;
		b -= 17;
	}

	int	c = 0, d = 0;
	for(int i=0; i<17; i++)
	{
		assert( 1000 * i == c);
		assert(-1000 * i == d);
		c += 1000;
		d -= 1000;
	}
	return 0;
}
