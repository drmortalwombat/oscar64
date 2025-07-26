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
				exit(-2);
		}
		free(memp[n]);
		
		s = rand() % 100 + 3;
		mems[n] = s;
		memp[n] = malloc(s);
		if (!memp[n])
			exit(-3);
		memset(memp[n], n, s);
	}
}

void callocckeck(void) {
	static const int ALLOC_COUNT = 4;
	static const int MAX_SIZE = 8;

    int i, j;
    int sizes[ALLOC_COUNT];
    char str[ALLOC_COUNT][2 * MAX_SIZE];
    void *ptr[ALLOC_COUNT];

    for (i = 0; i < ALLOC_COUNT; i++)
    {
        sizes[i] = (rand() % MAX_SIZE + 1) * 2;

		for (j=0; j < sizes[i]-1; j++) {
			str[i][j] = 'a' + rand() % 26;
		}

		str[i][sizes[i]-1] = '\0'; 
	}

    for (i = 0; i < ALLOC_COUNT; i++)
    {
        ptr[i] = calloc(sizes[i] / 2, 2);

        if (ptr[i] == NULL)
        {
			exit(-4);
		}

        strcpy(ptr[i], str[i]);
    }

    for (i = 0; i < ALLOC_COUNT; i++)
    {
        if (strcmp(ptr[i], str[i]) != 0)
        {
			exit(-5);
        }
    }

    for (i = 0; i < ALLOC_COUNT; i++)
    {
        free(ptr[i]);
    }
}

void divcheck(void) {
	div_t d = div(10, 3);
	if (d.quot != 3 || d.rem != 1)
		exit(-6);

	ldiv_t ld = ldiv(10, 3);
	if (ld.quot != 3 || ld.rem != 1)
		exit(-7);
}

int main(void)
{
	numchecks();
	heapcheck();
	callocckeck();
	divcheck();
	
	return 0;
}
