#include <assert.h>
#include <stdio.h>

int p1(int a, int b)
{
	return a + b;
}

int p2(int a, int b)
{
	return a * b;
}

int c1(int x)
{
	return x;
}

int c2(int x)
{
	return c1(x);
}

int main(void)
{
	return p1(5, p2(c2(2), c2(4))) - 13;
}

