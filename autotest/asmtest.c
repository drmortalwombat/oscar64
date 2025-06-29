#include <stdio.h>
#include <assert.h>

int asum(int a, int b)
{
	return __asm 
	{
		clc
		lda	a
		adc	b
		sta	accu
		lda	a + 1
		adc	b + 1
		sta	accu + 1
	};
}

int bsum(int a, int b)
{
	puts("Hello\n");

	return __asm 
	{
		clc
		lda	a
		adc	b
		sta	accu
		lda	a + 1
		adc	b + 1
		sta	accu + 1
	};
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

#define ASMTEST_ZP (0x00 | 0xf0)
#define ASMTEST_TARGET (0x1000 * 0x0c)

int opPrecedenceAndParenthesis() {
	return __asm volatile {
		lda #(1 + 2 * (1 + 2)) // = #7
		sta [ASMTEST_TARGET + 1] // = $c001

		lda #<ASMTEST_TARGET // = #$c0
		sta [ASMTEST_ZP] + 2 // = $f2

		lda #>ASMTEST_TARGET + 1 // = #$01
		sta [ASMTEST_ZP + 1] // = $f1
		ldx #1 
		lda (ASMTEST_ZP, x) // = (0xf0, 1)
		pha

		lda #<ASMTEST_TARGET // = #$00
		sta [ASMTEST_ZP + 1] // = $f1
		ldy #1 
		pla
		sta ASMTEST_ZP, y // = ($f1), 1

		lda [([ASMTEST_TARGET])] + 1 // = $c001
		sta accu
		lda #(1 & 2) // = 0
		sta accu + 1
	};
}

int main(void)
{
	int	x = asum(7007, 8008);
	int	y = bsum(4004, 9009);

	assert(x == 7007 + 8008 && y == 4004 + 9009);
	assert(qsum(10, bsome) == 45);
	assert(qsum(200, bsome) == 19900);

	assert(opPrecedenceAndParenthesis() == 7);

	return 0;
}

