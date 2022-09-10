#include <string.h>
#include <stdio.h>
#include <assert.h>

struct Node
{
	int		a;
	char	s[10];
};

void qsort(Node * n, int s)
{
	if (s > 1)
	{
		Node	pn = n[0];
		int 	pi = 0;
		for(int i=1; i<s; i++)
		{
			if (strcmp(n[i].s, pn.s) < 0)
			{
				n[pi] = n[i];
				pi++;
				n[i] = n[pi];
			}
		}
		n[pi] = pn;

		qsort(n, pi);
		qsort(n + pi + 1, s - pi - 1);
	}
}

void shuffle(Node * n, int s)
{
	for(int i=0; i<s; i++)
	{
		int t = rand() % s;
		Node	nt = n[i]; n[i] = n[t]; n[t] = nt;
	}
}

void init(Node * n, int s)
{
	for(int i=0; i<s; i++)
	{
		n[i].a = i;
		sprintf(n[i].s, "%.5d", i);
	}
}

static const int size = 1000;

Node	field[size];

void show(Node * n, int s)
{
	for(int i=0; i<s; i++)
	{
		printf("%3d : %3d, %s\n", i, n[i].a, n[i].s);
	}
	printf("\n");
}

void check(Node * n, int s)
{
	for(int i=0; i<s; i++)
	{
		assert(n[i].a == i);
	}
}

int main(void)
{
	init(field, size);
//	show(field, size);
	shuffle(field, size);
//	show(field, size);
	qsort(field, size);
//	show(field, size);
	check(field, size);

	return 0;
}