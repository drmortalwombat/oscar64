#include <assert.h>

struct S
{
	int a[101];
};

int lts50(S * s)
{
	int	x = 0;
	for(int i=0; i<50; i++)
		x += s->a[i];
	return x;
}

int les50(S * s)
{
	int	x = 0;
	for(int i=0; i<=49; i++)
		x += s->a[i];
	return x;
}

int gts50(S * s)
{
	int	x = 0;
	for(int i=50; i>0; i--)
		x += s->a[i-1];
	return x;
}

int ges50(S * s)
{
	int	x = 0;
	for(int i=49; i>=0; i--)
		x += s->a[i];
	return x;
}

int ltu50(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<50; i++)
		x += s->a[i];
	return x;
}

int leu50(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<=49; i++)
		x += s->a[i];
	return x;
}

int gtu50(S * s)
{
	unsigned	x = 0;
	for(int i=50; i>0; i--)
		x += s->a[i-1];
	return x;
}

int geu50(S * s)
{
	unsigned	x = 0;
	for(int i=50; i>=1; i--)
		x += s->a[i-1];
	return x;
}


int lts100(S * s)
{
	int	x = 0;
	for(int i=0; i<100; i++)
		x += s->a[i];
	return x;
}

int les100(S * s)
{
	int	x = 0;
	for(int i=0; i<=99; i++)
		x += s->a[i];
	return x;
}

int gts100(S * s)
{
	int	x = 0;
	for(int i=100; i>0; i--)
		x += s->a[i-1];
	return x;
}

int ges100(S * s)
{
	int	x = 0;
	for(int i=99; i>=0; i--)
		x += s->a[i];
	return x;
}

int ltu100(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<100; i++)
		x += s->a[i];
	return x;
}

int leu100(S * s)
{
	unsigned	x = 0;
	for(int i=0; i<=99; i++)
		x += s->a[i];
	return x;
}

int gtu100(S * s)
{
	unsigned	x = 0;
	for(int i=100; i>0; i--)
		x += s->a[i-1];
	return x;
}

int geu100(S * s)
{
	unsigned	x = 0;
	for(int i=100; i>=1; i--)
		x += s->a[i-1];
	return x;
}

void test50(void)
{
	S	s;

	int	k = 0;
	for(int i=0; i<50; i++)
	{
		int	t = (i & 15) + 3;
		s.a[i] = t;
		k += t;
	}
	s.a[50] = 1000;

	assert(lts50(&s) == k);
	assert(les50(&s) == k);
	assert(gts50(&s) == k);
	assert(ges50(&s) == k);
	assert(ltu50(&s) == k);
	assert(leu50(&s) == k);
	assert(gtu50(&s) == k);
	assert(geu50(&s) == k);
}

void test100(void)
{
	S	s;

	int	k = 0;
	for(int i=0; i<100; i++)
	{
		int	t = (i & 15) + 3;
		s.a[i] = t;
		k += t;
	}
	s.a[100] = 1000;

	assert(lts100(&s) == k);
	assert(les100(&s) == k);
	assert(gts100(&s) == k);
	assert(ges100(&s) == k);
	assert(ltu100(&s) == k);
	assert(leu100(&s) == k);
	assert(gtu100(&s) == k);
	assert(geu100(&s) == k);	
}

int main(void)
{
	test50();
	test100();

	return 0;
}
