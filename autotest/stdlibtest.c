// stdlibtest


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

void numcheck(int n)
{
	char	buffer[10];
	itoa(n, buffer, 10);
	int	m = atoi(buffer);
	
	printf("%d : %s -> %d\n", n, buffer, m);
	
	if (m != n)
		exit(-1);	
}

void numchecks(void)
{
	numcheck(0);
	numcheck(-1);
	numcheck(1);
	numcheck(12345);
	numcheck(-12345);
	numcheck(INT_MIN);
	numcheck(INT_MAX);
}

void heapcheck(void)
{
	void	*	memp[100];
	int			mems[100];
	int			n, k, s, i;
	
	for(n=0; n<100; n++)
	{
		s = rand() % 100 + 3;
		mems[n] = s;
		memp[n] = malloc(s);
		memset(memp[n], n, s);
	}
	
	for(k=0; k<1000; k++)
	{
		n = rand() % 100;
		int	s = mems[n];
		char	*	p = memp[n];
		for(i=0; i<s; i++)
		{
			if (p[i] != n)
			{
				printf("MemError %d at %d:%d != %d\n", k, i, n, p[i]);
				exit(-2);
			}
		}
		free(memp[n]);
		
		s = rand() % 100 + 3;
		mems[n] = s;
		memp[n] = malloc(s);
		memset(memp[n], n, s);
	}
}

int main(void)
{
	numchecks();
	heapcheck();
	
	return 0;
}
