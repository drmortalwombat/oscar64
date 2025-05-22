#include <assert.h>
#include <stdio.h>

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

void test_dcopy_init(void)
{
	n = 0;

	{
		C0	x(4);
		C0	y(x);
	}

	assert(n == 1 && t == -4);

	t = 0;
}

void test_copy_init(void)
{
	n = 0;

	{
		C1	x(4);
		C1	y(x);
	}

	assert(n == 2 && t == 0);
}

struct C2
{
	C1	a, b;

	C2(void);
};

C2::C2(void) : a(1), b(3)
{}

void test_minit(void)
{	
	n = 0;

	{
		C2	x;
	}

	assert(n == 2 && t == 0);
}

void test_minit_copy(void)
{
	n = 0;

	{
		C2	x;
		C2	y(x);
	}

	assert(n == 4 && t == 0);
}

int k;

void test_param_fv(C2 c)
{
	k += c.a.u;
}

void test_param_fr(C2 & c)
{
	k += c.a.u;
}

void test_param_value(void)
{
	n = 0;

	{
		C2	x;
		test_param_fv(x);
	}

	assert(n == 4 && t == 0);	
}

void test_param_ref(void)
{
	n = 0;

	{
		C2	x;
		test_param_fr(x);
	}

	assert(n == 2 && t == 0);	
}

C2 test_ret_v(void)
{
	C2	c;
	return c;
}

C2 & test_ret_r(C2 & r)
{
	return r;
}

void test_return_value(void)
{
	n = 0;

	{
		C2 c(test_ret_v());
	}

	assert(n == 6 && t == 0);
}

void test_return_reference(void)
{
	n = 0;

	{
		C2 d;		
		C2 c(test_ret_r(d));
	}

	assert(n == 2 && t == 0);
}

void test_retparam_value(void)
{
	n = 0;

	{
		test_param_fv(test_ret_v());
	}

	assert(n == 6 && t == 0);	
}

void test_retparam_reference(void)
{
	n = 0;

	{
		test_param_fr(test_ret_v());
	}

	assert(n == 4 && t == 0);	
}

int main(void)
{
	test_dcopy_init();
	test_copy_init();
	test_minit();
	test_minit_copy();
	test_param_value();
	test_param_ref();
	test_return_value();
	test_retparam_value();
	test_retparam_reference();

	return 0;
}