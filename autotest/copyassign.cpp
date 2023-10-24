#include <assert.h>

int t, n;

struct C0
{
	int u;

	C0(int a);
	~C0(void);
};

C0::C0(int a) : u(a)
{
	t += u;
	n++;
}

C0::~C0(void)
{
	t -= u;
}

struct C1
{
	int u;

	C1(int a);
	~C1(void);
	C1(const C1 & c);
	C1 & operator=(const C1 & c);
};

C1::C1(int a) : u(a)
{
	t += u;
	n++;
}

C1::~C1(void)
{
	t -= u;
}

C1::C1(const C1 & c) : u(c.u)
{
	t += u;
	n++;
}

C1 & C1::operator=(const C1 & c)
{
	t -= u;
	u = c.u;
	t += u;
	return *this;
}

void test_assign(void)
{
	n = 0;

	{
		C1	c(4);
		C1	d(5);
		c = d;
	}

	assert(n == 2 && t == 0);
}

struct C2
{
	C1		a, b;

	C2(int x, int y) : a(x), b(y)
		{}
};

void test_assign_deflt(void)
{
	n = 0;

	{
		C2	c(2, 3);
		C2	d(5, 10);
		c = d;
	}

	assert(n == 4 && t == 0);
}

int k;

C2 test_ret_v(void)
{
	C2	c(5, 10);
	return c;
}

C2 & test_ret_r(C2 & r)
{
	return r;
}

void test_assign_return_value(void)
{
	n = 0;

	{
		C2	c(2, 3);
		c = test_ret_v();
	}

	assert(n == 6 && t == 0);
}


int main(void)
{
	test_assign();
	test_assign_deflt();
	test_assign_return_value();

	return 0;
}
