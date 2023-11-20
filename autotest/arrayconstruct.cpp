#include <assert.h>

int t, n, m, k;

struct C1
{	
	int a;

	C1(void);
	~C1(void);
	C1(const C1 & c);
	C1 & operator=(const C1 & c);
};

struct C2
{
	C1		nc[10], mc[20];
};

C1::C1(void)
{
	a = 1;
	n++;
	t++;
}

C1::C1(const C1 & c)
{
	a = c.a;
	k++;
	t++;
}

C1 & C1::operator=(const C1 & c)
{
	a = c.a;
	m++;
	return *this;
}

C1::~C1(void)
{
	t--;
}

void test_local_init(void)
{
	n = 0;

	{
		C1	c[10];
	}

	assert(n == 10 && t == 0);
}

void test_member_init(void)
{
	n = 0;

	{
		C2		d;
	}

	assert(n == 30 && t == 0);
}

void test_member_array_init(void)
{
	n = 0;

	{
		C2		d[5];
	}

	assert(n == 150 && t == 0);
}

void test_local_copy(void)
{
	n = 0;
	k = 0;

	{
		C1	c[10];
		C1	d(c[4]);
	}

	assert(n == 10 && k == 1 && t == 0);
}

void test_member_copy(void)
{
	n = 0;
	k = 0;

	{
		C2		d;
		C2		e(d);
	}

	assert(n == 30 && k == 30 && t == 0);
}

void test_local_assign(void)
{
	n = 0;
	k = 0;
	m = 0;

	{
		C1	c[10];
		C1	d[5];

		d[4] = c[2];
	}

	assert(n == 15 && k == 0 && m == 1 && t == 0);
}

void test_member_assign(void)
{
	n = 0;
	k = 0;
	m = 0;

	{
		C2		d;
		C2		e;
		e = d;
	}

	assert(n == 60 && k == 0 && m == 30 && t == 0);
}


int main(void)
{
	test_local_init();
	test_member_init();

	test_member_array_init();

	test_local_copy();
	test_member_copy();

	test_local_assign();
	test_member_assign();

	return 0;
}

