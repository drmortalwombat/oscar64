#include <stdio.h>
#include <assert.h>

static const int size = 100;

int main(void)
{
	int	a[size];

	for(int i=0; i<size; i++)
		a[i] = i;

	int s = 0;
	for(int i=0; i<size; i++)
		s += a[i];

	assert(s == 4950);

	return 0;
}
