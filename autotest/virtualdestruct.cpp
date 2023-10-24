#include <assert.h>

int a, b, c;

struct A
{	
	A(void)
	{
		a++;
	}

	virtual ~A(void)
	{
		a--;
	}
};

struct B : A
{	
	B(void)
	{
		b++;
	}

	virtual ~B(void)
	{
		b--;
	}
};


struct C : B
{	
	C(void)
	{
		c++;
	}

	virtual ~C(void)
	{
		c--;
	}
};


int main()
{
	A *	t[3];

	t[0] = new A();
	t[1] = new B();
	t[2] = new C();

	assert(a == 3 && b == 2 && c == 1);

	delete t[0];
	delete t[1];
	delete t[2];

	assert(a == 0 && b == 0 && c == 0);

	return 0;
}
