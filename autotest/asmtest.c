#include <stdio.h>

int asum(int a, int b)
{
	__asm 
	{
		ldy	#a
		clc
		lda	(fp), y
		ldy	#b
		adc	(fp), y
		sta	accu
		ldy	#a + 1
		lda	(fp), y
		ldy	#b + 1
		adc	(fp), y
		sta	accu + 1
	}
}

int bsum(int a, int b)
{
	puts("Hello\n");

	__asm 
	{
		ldy	#a
		clc
		lda	(fp), y
		ldy	#b
		adc	(fp), y
		sta	accu
		ldy	#a + 1
		lda	(fp), y
		ldy	#b + 1
		adc	(fp), y
		sta	accu + 1
	}
}

int main(void)
{
	int	x = asum(7007, 8008);
	int	y = bsum(4004, 9009);

	return (x == 7007 + 8008 && y == 4004 + 9009) ? 0 : -1;
}

