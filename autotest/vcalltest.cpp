#include <assert.h>

struct A
{
	virtual int f(int x);
};

struct B : A
{
	virtual int f(int x);
};

struct C : A
{
	virtual int f(int x);
	virtual int g(int y, int z);
};

struct D : B
{
	virtual int f(int x);	
};

struct E : C
{
	virtual int f(int x);	
	virtual int g(int y, int z);
};

struct F : C
{
	virtual int g(int y, int z);
};

int A::f(int x)
{
	return x * 1;
}

int B::f(int x)
{
	return x * 2;
}

int C::f(int x)
{
	return x * 3;
}

int C::g(int y, int z)
{
	return (y + z) * 3;
}

int D::f(int x)
{
	return x * 4;
}

int E::f(int x)
{
	return x * 5;
}

int E::g(int y, int z)
{
	return (y + z) * 5;
}

int F::g(int y, int z)
{
	return (y + z) * 6;
}

int main(void)
{
	A 	a;
	B 	b;
	C 	c;
	D 	d;
	E 	e;
	F 	f;

	assert(a.f(3) == c.f(1));
	assert(b.f(4) == d.f(2));
	assert(e.f(2) == b.f(5));
	assert(f.f(5) == e.f(3));

	assert(c.g(3, 2) == e.g(1, 2));
	assert(c.g(1, 5) == f.g(0, 3));

	return 0;
}
