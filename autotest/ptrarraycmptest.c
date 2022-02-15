#include <assert.h>

struct X
{
	int	a;
};

X	x[5] = {
	{1}, {2}, {3}, {4}, {5}
};

X	*	y;

int main(void)
{
	y = x;
	assert(y == x);
	y = x + 1;
	assert(y == x + 1);
	y = &(x[2]);
	assert(y == x + 2);
	y = x + 3;
	assert(y == &(x[3]));
	y = x ;
	assert(y == (struct X*)x);

	y = x;
	assert(x == y);
	y = x + 1;
	assert(x + 1 == y);
	y = &(x[2]);
	assert(x + 2 == y);
	y = x + 3;
	assert(&(x[3]) == y);
	y = x ;
	assert((struct X*)x == y);

	return 0;
}
