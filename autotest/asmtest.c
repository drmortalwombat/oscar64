#include <stdio.h>
#include <assert.h>

int asum(int a, int b)
{
	__asm 
	{
		clc
		lda	a
		adc	b
		sta	accu
		lda	a + 1
		adc	b + 1
		sta	accu + 1
	}
}

int bsum(int a, int b)
{
	puts("Hello\n");

	__asm 
	{
		clc
		lda	a
		adc	b
		sta	accu
		lda	a + 1
		adc	b + 1
		sta	accu + 1
	}
}

int b, t[10];

int bsome(int x)
{
	return x;
}


int qsum(int a, int (* c)(int))
{
	char n = 0;
	b = 0;
	for(int i=0; i<a; i++)
	{
		int j = c((char)i);
		__asm
		{
			clc
			lda	j
			adc b
			sta b
			lda j + 1
			adc b + 1
			sta b + 1
		}
		t[n] += i;
		n = (n + 1) & 7;
	}
	return b;
}

int main(void)
{
	int	x = asum(7007, 8008);
	int	y = bsum(4004, 9009);

	assert(x == 7007 + 8008 && y == 4004 + 9009);
	assert(qsum(10, bsome) == 45);
	assert(qsum(200, bsome) == 19900);

	return 0;
}

