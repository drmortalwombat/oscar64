#include <assert.h>

int t, n;

struct C1
{
	int 	a;

	C1(int x);
	~C1(void);
};

C1::C1(int x) : a(x)
{
	t += a;
	n++;
}

C1::~C1(void)
{
	t -= a;
}

void test_base(void)
{
	n = 0;

	{
		C1	x(2);
		C1	y(1);
	}

	assert(t == 0 && n == 2);
}

void test_base_loop(void)
{
	n = 0;

	for(int i=0; i<10; i++)
	{
		C1	x(2);
		C1	y(1);
	}

	assert(t == 0 && n == 20);
}

struct C2
{
	C1	c, d;

	C2(void);
};

C2::C2(void) 
	: c(7), d(11)
{

}

void test_member(void)
{
	n = 0;

	{
		C2	x();
		C2	y();
	}

	assert(t == 0 && n == 4);
}

void test_member_loop(void)
{
	n = 0;

	for(int i=0; i<10; i++)
	{
		C2	x();
		C2	y();
	}

	assert(t == 0 && n == 40);
}

struct C3
{
	C2	x, y;
};

void test_default(void)
{
	n = 0;

	{
		C3	x();
		C3	y();
	}

	assert(t == 0 && n == 8);
}

void test_default_loop(void)
{
	n = 0;

	for(int i=0; i<10; i++)
	{
		C3	x();
		C3	y();
	}

	assert(t == 0 && n == 80);
}

inline void test_inline_x(void)
{
	C1	x(1), y(2);
}

void test_inline(void)
{
	n = 0;

	test_inline_x();

	assert(t == 0 && n == 2);
}

inline void test_inline_xr(void)
{
	C1	x(1), y(2);

	{
		C1	x(3);
		return;
	}
}

void test_inline_return(void)
{
	n = 0;

	test_inline_xr();

	assert(t == 0 && n == 3);
}

int main(void)
{
	test_base();
	test_base_loop();

	test_member();
	test_member_loop();

	test_default();
	test_default_loop();

	test_inline();
	test_inline_return();

	return 0;
}
