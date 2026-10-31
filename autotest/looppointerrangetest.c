#include <assert.h>

#define A ((volatile unsigned char *)0x4000)
#define B ((volatile unsigned char *)0x5000)

int main(void)
{
	for (unsigned i = 0; i < 1280; i++)
	{
		A[i] = 0;
		B[i] = 0;
	}

	// The final pointer increment crosses a page boundary.
	for (unsigned i = 768; i < 1024; i++)
	{
		A[i] = 0x5a;
		B[i] = 0xa5;
	}

	for (unsigned i = 0; i < 1280; i++)
	{
		unsigned char a = (i >= 768 && i < 1024) ? 0x5a : 0;
		unsigned char b = (i >= 768 && i < 1024) ? 0xa5 : 0;
		assert(A[i] == a);
		assert(B[i] == b);
	}
	return 0;
}
