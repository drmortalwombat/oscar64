#include <assert.h>

struct A
{
	char x;
	union {
		char a[2];
		int  b;
	};
	char y;
};

union B
{
	struct {
		int x, y;
	};
	long z;
};

union C
{
	struct {
		char a0 : 2;
		char a1 : 2;
		char a2 : 2;
		char a3 : 2;
	};
	char	b;
};

struct D
{
	char	a;
	struct {
		char b;
		struct {
			char c;
			char d;
		};
		char e;
	};
	char f;
};

void init(A * a, B * b, C * c, D * d)
{
	a->x = 1;
	a->y = 2;

	a->b = 0x4711;
	b->z = 0x01020304;

	c->a0 = 1;
	c->a1 = 0;
	c->a2 = 3;
	c->a3 = 2;

	d->a = 1;
	d->b = 2;
	d->c = 3;
	d->d = 4;
	d->e = 5;
	d->f = 6;
}


int main(void)
{
	A	a;
	B	b;
	C 	c;
	D 	d;

	init(&a, &b, &c, &d);

	assert(sizeof(A) == 4);
	assert(a.x == 1);
	assert(a.y == 2);
	assert(a.a[0] == 0x11);
	assert(a.a[1] == 0x47);

	assert(sizeof(B) == 4);
	assert(b.x == 0x0304);
	assert(b.y == 0x0102);

	assert(sizeof(C) == 1);
	assert(c.b == 0b10110001);

	C 	c2 = {1, 2, 3, 2};
	assert(c2.b == 0b10111001);

	assert(sizeof(D) == 6);
	assert(d.a == 1);
	assert(d.b == 2);
	assert(d.c == 3);
	assert(d.d == 4);
	assert(d.e == 5);
	assert(d.f == 6);

	D 	d2 = {1, 2, 3, 4, 5, 6};
	assert(d2.a == 1);
	assert(d2.b == 2);
	assert(d2.c == 3);
	assert(d2.d == 4);
	assert(d2.e == 5);
	assert(d2.f == 6);

}
