#include <assert.h>

struct A
{
	unsigned			a		:	9;

	char				b		:	5;
	char				c		:	2;

	signed char			d;
	char				e		:	4;
	unsigned			f		:	12;
};


__noinline A init(unsigned a, char b, char c, signed char d, char e, unsigned f)
{
	return (A){a, b, c, d, e, f};
}

A xa[10];

__noinline void inita(int i, unsigned a, char b, char c, signed char d, char e, unsigned f)
{
	xa[i] = (A){a, b, c, d, e, f};
}

__noinline void initr(A & p, unsigned a, char b, char c, signed char d, char e, unsigned f)
{
	p = (A){a, b, c, d, e, f};
}

int main(void)
{
	A	x(init(1, 2, 3, 4, 5, 6));

	assert(x.a == 1);
	assert(x.b == 2);
	assert(x.c == 3);
	assert(x.d == 4);
	assert(x.e == 5);
	assert(x.f == 6);

	A	y(init(0x101, 0x11, 0, -127, 9, 0x801));

	assert(y.a == 0x101);
	assert(y.b == 0x11);
	assert(y.c == 0);
	assert(y.d == -127);
	assert(y.e == 9);
	assert(y.f == 0x801);

	inita(2, 1, 2, 3, 4, 5, 6);

	assert(xa[2].a == 1);
	assert(xa[2].b == 2);
	assert(xa[2].c == 3);
	assert(xa[2].d == 4);
	assert(xa[2].e == 5);
	assert(xa[2].f == 6);

	inita(3, 0x101, 0x11, 0, -127, 9, 0x801);

	assert(xa[3].a == 0x101);
	assert(xa[3].b == 0x11);
	assert(xa[3].c == 0);
	assert(xa[3].d == -127);
	assert(xa[3].e == 9);
	assert(xa[3].f == 0x801);

	initr(xa[4], 1, 2, 3, 4, 5, 6);

	assert(xa[4].a == 1);
	assert(xa[4].b == 2);
	assert(xa[4].c == 3);
	assert(xa[4].d == 4);
	assert(xa[4].e == 5);
	assert(xa[4].f == 6);

	initr(xa[5], 0x101, 0x11, 0, -127, 9, 0x801);

	assert(xa[5].a == 0x101);
	assert(xa[5].b == 0x11);
	assert(xa[5].c == 0);
	assert(xa[5].d == -127);
	assert(xa[5].e == 9);
	assert(xa[5].f == 0x801);

	return 0;
}
