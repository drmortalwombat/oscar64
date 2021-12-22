#include <assert.h>

struct S
{
	int a[100];
};

int lts(S * s)
{
	int	x = 0;
	for(int i=0; i<100; i++)
		x += s->a[i];
	return x;
}

int les(S * s)
{
	int	x = 0;
	for(int i=0; i<=99; i++)
		x += s->a[i];
	return x;
}

int gts(S * s)
{
	int	x = 0;
	for(int i=100; i>0; i--)
		x += s->a[i-1];
	return x;
}

int ges(S * s)
{
	int	x = 0;
	for(int i=99; i>=0; i--)
		x += s->a[i];
	return x;
}

int ltu(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<100; i++)
		x += s->a[i];
	return x;
}

int leu(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<=99; i++)
		x += s->a[i];
	return x;
}

int gtu(S * s)
{
	unsigned	x = 0;
	for(int i=100; i>0; i--)
		x += s->a[i-1];
	return x;
}

int geu(S * s)
{
	unsigned	x = 0;
	for(int i=100; i>=1; i--)
		x += s->a[i-1];
	return x;
}

int main(void)
{
	S	s;

	int	k = 0;
	for(int i=0; i<100; i++)
	{
		int	t = (i & 15) + 3;
		s.a[i] = t;
		k += t;
	}

	assert(lts(&s) == k);
	assert(les(&s) == k);
	assert(gts(&s) == k);
	assert(ges(&s) == k);
	assert(ltu(&s) == k);
	assert(leu(&s) == k);
	assert(gtu(&s) == k);
	assert(geu(&s) == k);

	return 0;
}
