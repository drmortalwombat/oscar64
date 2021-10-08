#include <stdio.h>

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

int main(void)
{
	int	x = asum(7007, 8008);
	int	y = bsum(4004, 9009);

	return (x == 7007 + 8008 && y == 4004 + 9009) ? 0 : -1;
}

