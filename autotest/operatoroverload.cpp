#include <assert.h>

struct A
{
	int n;

	A(int n_)
	: n(n_) {}

	A(const A & a)
	: n(a.n) {}

	A operator+(const A & a) const
	{
		return A(n + a.n);
	}

	A operator-(const A & a) const
	{
		return A(n - a.n);
	}

	A & operator+=(const A & a)
	{
		n += a.n;
		return *this;
	}

	A & operator-=(const A & a)
	{
		n -= a.n;
		return *this;
	}

	A operator-(void) const
	{
		return A(-n);
	}

	A & operator++(void)
	{
		n++;
		return *this;
	}

	A & operator--(void)
	{
		n--;
		return *this;
	}

	A operator++(int);

	A operator--(int);

};

A A::operator++(int)
{
	A a(*this);
	n++;
	return a;
}

A A::operator--(int)
{
	A a(*this);
	n--;
	return a;
}

int main(void)
{
	A a(7), b(8), c(9);

	assert((++a).n == 8);
	assert(a.n == 8);

	assert((--a).n == 7);
	assert(a.n == 7);

	assert((a++).n == 7);
	assert(a.n == 8);
	assert((a--).n == 8);
	assert(a.n == 7);

	assert((a + b - c + -a + -b - -c).n == 0);

	return 0;
}
