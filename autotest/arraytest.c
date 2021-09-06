#include <assert.h>
#include <stdio.h>

int sum(int * a, int s)
{
	int sum = 0;
	for(int i=0; i<s; i++)
	{
		sum += a[i];
	}
	return sum;
}

int main(void)
{
	int	a[100];
	for(int i=0; i<100; i++)
	{
		a[i] = i % 10;
	}
	assert(sum(a, 100) == 450);
	return 0;
}
