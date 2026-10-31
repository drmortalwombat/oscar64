#include <assert.h>

#define A ((volatile unsigned char *)0x4000)
#define B ((volatile unsigned char *)0x5000)

#define A1 ((volatile unsigned char *)0x4001)
#define B1 ((volatile unsigned char *)0x5001)

__noinline void test_align(void)
{
	// The final pointer increment crosses a page boundary.
	for (unsigned i = 768; i < 1024; i++)
	{
		A[i] = 0x5a;
		B[i] = 0xa5;
	}
}

__noinline void test_misalign_pointer(void)
{
	// The final pointer increment crosses a page boundary.
	for (unsigned i = 768; i < 1024; i++)
	{
		A1[i] = 0x5a;
		B1[i] = 0xa5;
	}
}

__noinline void test_misalign_index(void)
{
	// The final pointer increment crosses a page boundary.
	for (unsigned i = 769; i < 1025; i++)
	{
		A[i] = 0x5a;
		B[i] = 0xa5;
	}
}

int main(void)
{
	for (unsigned i = 0; i < 1280; i++)
	{
		A[i] = 0;
		B[i] = 0;
	}

	test_align();

	for (unsigned i = 0; i < 1280; i++)
	{
		unsigned char a = (i >= 768 && i < 1024) ? 0x5a : 0;
		unsigned char b = (i >= 768 && i < 1024) ? 0xa5 : 0;
		assert(A[i] == a);
		assert(B[i] == b);
	}

	for (unsigned i = 0; i < 1280; i++)
	{
		A1[i] = 0;
		B1[i] = 0;
	}

	test_misalign_pointer();

	for (unsigned i = 0; i < 1280; i++)
	{
		unsigned char a = (i >= 768 && i < 1024) ? 0x5a : 0;
		unsigned char b = (i >= 768 && i < 1024) ? 0xa5 : 0;
		assert(A1[i] == a);
		assert(B1[i] == b);
	}

	for (unsigned i = 0; i < 1280; i++)
	{
		A[i] = 0;
		B[i] = 0;
	}

	test_misalign_index();

	for (unsigned i = 0; i < 1280; i++)
	{
		unsigned char a = (i >= 769 && i < 1025) ? 0x5a : 0;
		unsigned char b = (i >= 769 && i < 1025) ? 0xa5 : 0;
		assert(A[i] == a);
		assert(B[i] == b);
	}


	return 0;
}
