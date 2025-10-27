#include <assert.h>
#include <string.h>

struct A
{
	char	a, b;
	int		c;
	long	d;
};

struct B
{
	char	a[3], b;
	int		c;
	long	d;
};

__noinline void ssplit1(char t)
{
	A 	x;
	x.a = 12;
	x.b = 43;
	x.c = 100;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a == 22);
}

__noinline void ssplit2(char t)
{
	A 	x, y;
	x.a = 12;
	x.b = 43;
	x.c = 100;
	y.a = 12;
	char * q = &y.a;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a == 22);
	assert(*q == 12);
}


__noinline void nossplit1(char t)
{
	A 	x, y;
	x.a = 12;
	x.b = 43;
	x.c = 100;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	y = x;
	assert(y.c == t * 10 + 100);
	assert(y.a == 22);
}

__noinline void nossplit2(char t)
{
	A 	x, y;
	x.a = 12;
	x.b = 43;
	x.c = 100;
	y = x;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a == 22);
}

__noinline void nossplit3(char t)
{
	A 	x;
	x.a = 12;
	x.b = 43;
	x.c = 100;
	char * q = &x.a;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	assert(x.c == t * 10 + 100);
	assert(*q == 22);
}

__noinline void nossplit4(char t)
{
	A 	x = {12, 43, 100};
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a == 22);
}

__noinline void asplit1(char t)
{
	int 	x[4];
	x[0] = 12;
	x[1] = 43;
	x[2] = 100;
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	assert(x[2] == t * 10 + 100);
	assert(x[0] == 22);
}

__noinline void asplit2(char t)
{
	int 	x[4], y[4];
	x[0] = 12;
	x[1] = 43;
	x[2] = 100;
	y[0] = 12;
	int * q = &y[0];
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	assert(x[2] == t * 10 + 100);
	assert(x[0] == 22);
	assert(*q == 12);
}


__noinline void noasplit1(char t)
{
	int 	x[4], y[4];
	x[0] = 12;
	x[1] = 43;
	x[2] = 100;
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	memcpy(y, x, sizeof(y));
	assert(y[2] == t * 10 + 100);
	assert(y[0] == 22);
}

__noinline void noasplit2(char t)
{
	int 	x[4], y[4];
	x[0] = 12;
	x[1] = 43;
	x[2] = 100;
	memcpy(y, x, sizeof(y));
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	assert(x[2] == t * 10 + 100);
	assert(x[0] == 22);
}

__noinline void noasplit3(char t)
{
	int 	x[4];
	x[0] = 12;
	x[1] = 43;
	x[2] = 100;
	int * q = &x[0];
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	assert(x[2] == t * 10 + 100);
	assert(*q == 22);
}

__noinline void noasplit4(char t)
{
	int 	x[4] = {12, 43, 100};
	for(int i=0; i<10; i++)
	{
		x[2] += t;
		x[0]++;
	}
	assert(x[2] == t * 10 + 100);
	assert(x[0] == 22);
}


__noinline void sasplit1(char t)
{
	B 	x;
	x.a[1] = 12;
	x.b = 43;
	x.c = 100;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a[1] == 22);
}

__noinline void sasplit2(char t)
{
	B 	x, y;
	x.a[1] = 12;
	x.b = 43;
	x.c = 100;
	y.a[1] = 12;
	char * q = &y.a[1];
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a[1] == 22);
	assert(*q == 12);
}


__noinline void nosasplit1(char t)
{
	B 	x, y;
	x.a[1] = 12;
	x.b = 43;
	x.c = 100;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	y = x;
	assert(y.c == t * 10 + 100);
	assert(y.a[1] == 22);
}

__noinline void nosasplit2(char t)
{
	B 	x, y;
	x.a[1] = 12;
	x.b = 43;
	x.c = 100;
	y = x;
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a[1] == 22);
}

__noinline void nosasplit3(char t)
{
	B 	x;
	x.a[1] = 12;
	x.b = 43;
	x.c = 100;
	char * q = &x.a[1];
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	assert(x.c == t * 10 + 100);
	assert(*q == 22);
}

__noinline void nosasplit4(char t)
{
	B 	x = {{4, 12, 7}, 43, 100};
	for(int i=0; i<10; i++)
	{
		x.c += t;
		x.a[1]++;
	}
	assert(x.c == t * 10 + 100);
	assert(x.a[1] == 22);
}


int main(void)
{
	ssplit1(11); ssplit1(33);
	ssplit2(11); ssplit2(33);

	nossplit1(11); nossplit1(33);
	nossplit2(11); nossplit2(33);
	nossplit3(11); nossplit3(33);
	nossplit4(11); nossplit4(33);

	asplit1(11); asplit1(33);
	asplit2(11); asplit2(33);

	noasplit1(11); noasplit1(33);
	noasplit2(11); noasplit2(33);
	noasplit3(11); noasplit3(33);
	noasplit4(11); noasplit4(33);

	sasplit1(11); sasplit1(33);
	sasplit2(11); sasplit2(33);

	nosasplit1(11); nosasplit1(33);
	nosasplit2(11); nosasplit2(33);
	nosasplit3(11); nosasplit3(33);
	nosasplit4(11); nosasplit4(33);

	return 0;
}