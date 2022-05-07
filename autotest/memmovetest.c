#include <string.h>
#include <assert.h>
#include <stdio.h>

unsigned b[4096];

void testfwd(unsigned sz)
{
	for(unsigned i=0; i<4096; i++)
		b[i] = i;

	memmove(b + 100, b + 101, 2 * sz);

	for(unsigned i=0; i<100; i++)
		assert(b[i] == i);
	for(unsigned i=100; i<100 + sz; i++)
		assert(b[i] == i + 1);
	for(unsigned i=100 + sz; i<4096; i++)
		assert(b[i] == i);
}

void testback(unsigned sz)
{
	for(unsigned i=0; i<4096; i++)
		b[i] = i;

	memmove(b + 101, b + 100, 2 * sz);

	for(unsigned i=0; i<101; i++)
		assert(b[i] == i);
	for(unsigned i=101; i<101 + sz; i++)
		assert(b[i] == i - 1);
	for(unsigned i=101 + sz; i<4096; i++)
		assert(b[i] == i);
}

int main(void)
{
	for(unsigned i=1; i<2048; i *= 2)
	{
		testfwd(i - 1);
		testfwd(i);
		testback(i);
		testback(i - 1);
	}

	return 0;
}

