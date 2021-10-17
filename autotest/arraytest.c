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

void copy(int * d, const int * s, int n)
{
	for(int i=0; i<n ; i++)
		d[i] = s[i];
}

void reverse(int * d, const int * s, int n)
{
	for(int i=0; i<n ; i++)
		d[i] = s[n - i - 1];
}

int sumb(int * a, char s)
{
	int sum = 0;
	for(char i=0; i<s; i++)
	{
		sum += a[i];
	}
	return sum;
}

void copyb(int * d, const int * s, char n)
{
	for(char i=0; i<n ; i++)
		d[i] = s[i];
}

void reverseb(int * d, const int * s, char n)
{
	for(char i=0; i<n ; i++)
		d[i] = s[n - i - 1];
}

int main(void)
{
	int	a[100], b[100], c[100];
	for(int i=0; i<100; i++)
	{
		a[i] = i % 10;
	}

	assert(sum(a, 100) == 450);
	copy(b, a, 100);
	assert(sum(b, 100) == 450);
	reverse(c, a, 100);
	assert(sum(c, 100) == 450);

	assert(sumb(a, 100) == 450);
	copyb(b, a, 100);
	assert(sumb(b, 100) == 450);
	reverseb(c, a, 100);
	assert(sumb(c, 100) == 450);

	return 0;
}
