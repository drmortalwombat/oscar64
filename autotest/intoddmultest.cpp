#include <assert.h>

template<int n>
__noinline int mul(int m)
{
	return m * n;
}

template<int n>
__noinline int mul32(char m)
{
	__assume(m < 32);
	return m * n;
}

template<int n>
__noinline int mul64(char m)
{
	__assume(m < 64);
	return m * n;
}

template<int n>
__noinline int mul128(char m)
{
	__assume(m < 128);
	return m * n;
}

template<int n>
__noinline int muls32(int m)
{
	__assume(m >= -32 && m < 32);
	return m * n;
}

template<int n>
__noinline int muls64(int m)
{
	__assume(m >= - 64 && m < 64);
	return m * n;
}

template<int n>
__noinline int muls128(int m)
{
	__assume(m >= -128 && m < 128);
	return m * n;
}

template<int n>
__noinline int mul256(char m)
{
	return m * n;
}

template<int n>
__noinline void test(void)
{
	int k = -32768 / n;
	for(int i=k * n; i<32700; i+=n)
	{
		assert(mul<n>(k) == i);
		k++;
	}

	k = 0;
	for(int i=0; i<32; i+=n)
	{
		assert(mul32<n>(k) == i);
		k++;
	}

	k = 0;
	for(int i=0; i<64; i+=n)
	{
		assert(mul64<n>(k) == i);
		k++;
	}

	k = 0;
	for(int i=0; i<128; i+=n)
	{
		assert(mul128<n>(k) == i);
		k++;
	}

	k = 0;
	for(int i=0; i<256; i+=n)
	{
		assert(mul256<n>(k) == i);
		k++;
	}

	k = -32 / n;
	for(int i=k * n; i<32; i+=n)
	{
		assert(muls32<n>(k) == i);
		k++;
	}

	k = -64 / n;
	for(int i=k * n; i<64; i+=n)
	{
		assert(muls64<n>(k) == i);
		k++;
	}

	k = -128 / n;
	for(int i=k * n; i<128; i+=n)
	{
		assert(muls128<n>(k) == i);
		k++;
	}
}

int main(void)
{
	test<3>();
	test<5>();
	test<7>();
	test<9>();
	test<10>();
}
