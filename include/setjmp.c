#include "setjmp.h"

int setjmpsp(jmp_buf env, void * sp)
{
	__asm
	{
		ldy	#0
		lda sp + 0
		sta (env), y
		iny
		lda sp + 1
		sta (env), y
		iny

		lda __ip + 0
		sta (env), y
		iny
		lda __ip + 1
		sta (env), y
		iny

		lda __fp + 0
		sta (env), y
		iny
		lda __fp + 1
		sta (env), y
		iny

		ldx #0
	l1:
		lda __sregs, x
		sta (env), y
		iny
		inx
		cpx #32
		bne l1

		tsx
		txa
		sta (env), y

		inx
	l2:
		iny 
		lda $0100, x
		sta (env), y
		inx
		bne l2
	}

	return 0;	
}

int setjmp(jmp_buf env)
{
	__asm
	{
		ldy	#0
		lda __sp + 0
		sta (env), y
		iny
		lda __sp + 1
		sta (env), y
		iny

		lda __ip + 0
		sta (env), y
		iny
		lda __ip + 1
		sta (env), y
		iny

		lda __fp + 0
		sta (env), y
		iny
		lda __fp + 1
		sta (env), y
		iny

		ldx #0
	l1:
		lda __sregs, x
		sta (env), y
		iny
		inx
		cpx #32
		bne l1

		tsx
		txa
		sta (env), y

		inx
	l2:
		iny 
		lda $0100, x
		sta (env), y
		inx
		bne l2
	}

	return 0;
}

#pragma native(setjmp)

void longjmp(jmp_buf env, int value)
{
	__asm
	{
		ldy	#0
		lda (env), y
		sta __sp + 0
		iny
		lda (env), y
		sta __sp + 1
		iny

		lda (env), y
		sta __ip + 0
		iny
		lda (env), y
		sta __ip + 1
		iny
		
		lda (env), y
		sta __fp + 0
		iny
		lda (env), y
		sta __fp + 1
		iny
		
		ldx #0
	l1:
		lda (env), y
		sta __sregs, x
		iny
		inx
		cpx #32
		bne l1

		lda (env), y
		tax
		txs

		inx
	l2:
		iny
		lda (env), y
		sta $0100, x
		inx
		bne l2

		lda value
		sta __accu
		lda value + 1
		sta __accu + 1
	}
}

#pragma native(longjmp)
