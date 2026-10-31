#include <assert.h>

#define A ((volatile unsigned char *)0x4000)
#define B ((volatile unsigned char *)0x5000)

__noinline void fill(void)
{
	for (unsigned i = 768; i < 1024; i++)
	{
		A[i] = 0x5a;
		B[i] = 0xa5;
	}
}

__noinline void keep_pointer(void)
{
	volatile unsigned char *p = A;
	for (unsigned i = 768; i < 1024; i++)
	{
		p = A + i;
		*p = 0x5a;
		B[i] = 0xa5;
	}
	*p = 0x7e;
}

__noinline void store_index(void)
{
	for (unsigned i = 768; i < 1024; i++)
	{
		A[i] = (unsigned char)i;
		B[i] = (unsigned char)(i >> 8);
	}
}

int main(void)
{
	for (unsigned i = 0; i < 1280; i++)
	{
		A[i] = 0;
		B[i] = 0;
	}
	fill();
	for (unsigned i = 0; i < 1280; i++)
	{
		assert(A[i] == ((i >= 768 && i < 1024) ? 0x5a : 0));
		assert(B[i] == ((i >= 768 && i < 1024) ? 0xa5 : 0));
	}
	keep_pointer();
	for (unsigned i = 0; i < 1280; i++)
	{
		assert(A[i] == (i == 1023 ? 0x7e : (i >= 768 && i < 1024) ? 0x5a : 0));
		assert(B[i] == ((i >= 768 && i < 1024) ? 0xa5 : 0));
	}
	store_index();
	for (unsigned i = 0; i < 1280; i++)
	{
		assert(A[i] == ((i >= 768 && i < 1024) ? (unsigned char)i : 0));
		assert(B[i] == ((i >= 768 && i < 1024) ? 3 : 0));
	}
	return 0;
}
