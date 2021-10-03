#include "time.h"

clock_t clock(void)
{
	__asm
	{
		lda $a2
		sta accu + 0
		lda $a1
		sta accu + 1
		lda $a0
		sta accu + 2
		lda #0
		sta accu + 3
	}
}
