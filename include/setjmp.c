#include "setjmp.h"

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
	loop:
		lda __sregs, x
		sta (env), y
		iny
		inx
		cpx #32
		bne loop

		tsx
		txa
		sta (env), y
		iny 

		lda $0101, x
		sta (env), y
		iny

		lda $0102, x
		sta (env), y
		iny
	}

	return 0
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
	loop:
		lda (env), y
		sta __sregs, x
		iny
		inx
		cpx #32
		bne loop

		lda (env), y
		tax
		txs
		iny 

		lda (env), y
		sta $0101, x
		iny

		lda (env), y
		sta $0102, x
		iny

		lda value
		sta __accu
		lda value + 1
		sta __accu + 1
	}
}

#pragma native(longjmp)
