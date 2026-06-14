#include <assert.h>

struct A
{
	char		x;
	unsigned	y;
	long		z;
};

__striped	A	a[4];

__striped	A	c[4] = {
	{1, 1000, 100000},
	{2, 2000, 200000},
	{3, 3000, 300000},
	{4, 4000, 400000}
};

__noinline long test_read(int k, char v)
{
	A 	t = c[k];

	if (t.x == v)
		return t.y;
	else
		return t.z;
}

__noinline long test_write(int k, char v, long l)
{
	A 	t = a[k];

	if (t.x == v)
		t.y = l;
	else
		t.z = l;

	return t.z;
}

int main(void)
{
	assert(test_read(0, 1) == 1000);
	assert(test_read(0, 0) == 100000);
	assert(test_read(2, 3) == 3000);
	assert(test_read(2, 0) == 300000);

	for(char i=0; i<4; i++)
		a[i] = c[i];

	assert(test_write(0, 1, 4711) == 100000);
	assert(test_write(0, 0, 4711) == 4711);
	assert(test_write(2, 3, 4711) == 300000);
	assert(test_write(2, 0, 4711) == 4711);

	return 0;
}
