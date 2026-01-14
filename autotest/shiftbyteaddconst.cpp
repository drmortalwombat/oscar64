#include <assert.h>

__noinline int base_add(int add, int shl, char a)
{
	return (a << shl) + add;
}

template<int add, int shl>
void test_add(char a)
{
	assert((a << shl) + add == base_add(add, shl, a));
}

template<int add>
void test_adds(char a)
{
	#for(i, 16) test_add<add, i>(a);
}

__noinline int base_sub(int sub, int shl, char a)
{
	return (a << shl) - sub;
}

template<int sub, int shl>
void test_sub(char a)
{
	assert(int(a << shl) - sub == base_sub(sub, shl, a));
}

template<int sub>
void test_subs(char a)
{
	#for(i, 16) test_sub<sub, i>(a);
}


int main(void)
{
	for(int i=0; i<256; i++)
		test_adds<15>(i);
	for(int i=0; i<256; i++)
		test_adds<16>(i);
	for(int i=0; i<256; i++)
		test_adds<111>(i);
	for(int i=0; i<256; i++)
		test_adds<4096>(i);
	for(int i=0; i<256; i++)
		test_adds<13421>(i);
	for(int i=0; i<256; i++)
		test_subs<15>(i);
	for(int i=0; i<256; i++)
		test_subs<16>(i);
	for(int i=0; i<256; i++)
		test_subs<4096>(i);
}

