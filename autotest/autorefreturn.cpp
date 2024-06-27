#include <stdio.h>
#include <assert.h>

struct X
{
	char	t;
	long	l;
};

__striped	X	xs[16];
X	xf[16];
char xp;

__noinline long tt(long n)
{
	return n;
}

inline auto & tas(void)
{
	return xs[xp].l;
}

inline auto & taf(void)
{
	return xf[xp].l;
}

long ts(char n)
{
	return tt(tas());
}

long tf(char n)
{
	return tt(taf());
}

inline auto bas(void)
{
	return xs[xp].l;
}

inline auto baf(void)
{
	return xs[xp].l;
}

long bs(char n)
{
	return tt(bas());
}

long bf(char n)
{
	return tt(baf());
}

int main(void)
{
	for(char i=0; i<16; i++)
	{
		xs[i].l = i * i;
		xf[i].l = i * i;
	}

	for(char i=0; i<16; i++)
	{
		xp = i;
		assert(ts(0) == i * i);
		assert(tf(0) == i * i);
		assert(bs(0) == i * i);
		assert(bf(0) == i * i);
	}

	return 0;
}
