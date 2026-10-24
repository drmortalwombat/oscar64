#include <assert.h>

#define A ((volatile unsigned char *)0x4000)
#define B ((volatile unsigned char *)0x5000)

int main(void)
{
	for (unsigned i = 0; i < 1024; i++)
	{
		A[i] = 0;
		B[i] = 0;
	}

	// The loop counter also becomes the low byte of an indirect pointer.
	for (unsigned i = 800; i < 1000; i++)
	{
		A[i] = 0x5a;
		B[i] = 0xa5;
	}

	for (unsigned i = 0; i < 1024; i++)
	{
		unsigned char a = (i >= 800 && i < 1000) ? 0x5a : 0;
		unsigned char b = (i >= 800 && i < 1000) ? 0xa5 : 0;
		assert(A[i] == a);
		assert(B[i] == b);
	}
	return 0;
}
