#include <assert.h>

void testmuli(long a, long b, long ab)
{
	assert (a * b == ab);
}

void testdivi(long a, long b, long ab)
{
	assert (a / b == ab);
}

void shltesti(long a, long b, long ab)
{
	assert (a << b == ab);
}

void shrtesti(long a, long b, long ab)
{
	assert (a >> b == ab);
}

long sieve(long size)
{
	bool	sieve[1000];
	
	for(long i=0; i<size; i+=2)
	{
		sieve[i] = false;
		sieve[i+1] = true;
	}
	sieve[2] = true;
	
	for (long i = 3; i * i < size;)
	{
		long j = i * i;
		while (j < size)
		{
			sieve[j] = false;
			j += 2 * i;
		}
		do {i++;} while (i < size && !sieve[i]);
	}	
	
	long	num = 0;
	for(long i=0; i<size; i++)
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

	testmuli( 1237,  1024, 1266688l);
	testmuli(-1237,  1024, -1266688l);
	testmuli( 1237, -1024, -1266688l);
	testmuli(-1237, -1024, 1266688l);

	testmuli(  1024, 1237, 1266688l);
	testmuli(  1024,-1237, -1266688l);
	testmuli( -1024, 1237, -1266688l);
	testmuli( -1024,-1237, 1266688l);

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

	long	a = 0, b = 0;
	for(long i=0; i<10000; i++)
	{
		assert( 177 * i == a);
		assert(-177 * i == b);
		a += 177;
		b -= 177;
	}

	long	c = 0, d = 0;
	for(long i=0; i<177; i++)
	{
		assert( 10000 * i == c);
		assert(-10000 * i == d);
		c += 10000;
		d -= 10000;
	}

	long	e = 0, f = 0;
	for(long i=0; i<177000l; i += 1000)
	{
		assert( 1024 * i == e);
		assert(-1024 * i == f);
		e += 1024000l;
		f -= 1024000l;
	}

	return 0;
}
