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

long suml(long * a, char s)
{
	long sum = 0;
	for(char i=0; i<s; i++)
	{
		sum += a[i];
	}
	return sum;
}

void copyl(long * d, const long * s, char n)
{
	for(char i=0; i<n ; i++)
		d[i] = s[i];
}

void reversel(long * d, const long * s, char n)
{
	for(char i=0; i<n ; i++)
		d[i] = s[n - i - 1];
}

int main(void)
{
	int	a[100], b[100], c[100];
	long al[100], bl[100], cl[100];

	for(int i=0; i<100; i++)
	{
		a[i] = i % 10;
		al[i] = i % 10;
	}
#if 0
	assert(sum(a, 100) == 450);
	copy(b, a, 100);
	assert(sum(b, 100) == 450);
	reverse(c, a, 100);
	assert(sum(c, 100) == 450);
	assert(sumb(a, 100) == 450);
#endif
	copyb(b, a, 100);
	assert(sumb(b, 100) == 450);
#if 0
	reverseb(c, a, 100);
	assert(sumb(c, 100) == 450);

	assert(suml(al, 100) == 450);

	copyl(bl, al, 100);
	assert(suml(bl, 100) == 450);

	reversel(cl, al, 100);
	assert(suml(cl, 100) == 450);
#endif
	return 0;
}
