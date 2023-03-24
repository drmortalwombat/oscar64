#include "time.h"

clock_t clock(void)
{
	__asm
	{
#if defined(__PLUS4__)
		lda $a5
		sta accu + 0
		lda $a4
		sta accu + 1
		lda $a3
		sta accu + 2
		lda #0
		sta accu + 3
#elif defined(__CBMPET__)
		lda $8f
		sta accu + 0
		lda $8e
		sta accu + 1
		lda $8d
		sta accu + 2
		lda #0
		sta accu + 3
#elif defined(__ATARI__)
loop:
		lda $14
		ldx $13
		ldy $12
		cmp $14
		bne loop
		
		sta accu + 0
		stx accu + 1
		sty accu + 2
		lda #0
		sta accu + 3		
#elif defined(__X16__)
		jsr $ffde
		sta accu + 0
		stx accu + 1
		sty accu + 2
		lda #0
		sta accu + 3
#else
		lda $a2
		sta accu + 0
		lda $a1
		sta accu + 1
		lda $a0
		sta accu + 2
		lda #0
		sta accu + 3
#endif
	}
}
