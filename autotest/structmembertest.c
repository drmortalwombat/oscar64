#include <assert.h>

struct A
{
	int	x[100], y[100];
};

struct V
{
	int	x, y, z;
};


void copy(A * a)
{
	for(int i=0; i<100; i++)
		a->x[i] = a->y[i];
}

void shuffle(V * v)
{
	for(int i=0; i<100; i++)
		v[i].x = v[i].y;
}

int main(void)
{
	A	a;
	V	v[100];
	
	for(int i=0; i<100; i++)
	{
		a.y[i] = i;
		v[i].y = i;
	}
	
	copy(&a);
	shuffle(v);

	for(int i=0; i<100; i++)
	{
		assert(a.x[i] == i);
		assert(v[i].x == i);
	}
	
	return 0;
}



