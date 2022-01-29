#include "fixmath.h"

unsigned long lmul16u(unsigned x, unsigned y)
{
	__asm
	{
		lda	#0
		sta	accu + 2
		sta	accu + 3

		ldx	#16
L1:		lsr	x + 1
		ror	x
		bcc	W1
		clc
		lda	accu + 2
		adc	y
		sta	accu + 2
		lda	accu + 3
		adc y + 1
		sta	accu + 3
W1:
		ror accu + 3
		ror accu + 2
		ror accu + 1
		ror accu
		dex
		bne	L1
	}
}

long lmul16s(int x, int y)
{
	__asm
	{
		bit y + 1
		bpl W0

		sec
		lda #0
		sbc y
		sta y
		lda #0
		sbc y + 1
		sta y + 1

		sec
		lda #0
		sbc x
		sta x
		lda #0
		sbc x + 1
		sta x + 1
W0:
		ldx	#15
		lda #0
		sta	accu + 2

L1:		lsr	x + 1
		ror	x
		bcc	W1
		tay
		clc
		lda	accu + 2
		adc	y
		sta accu + 2
		tya
		adc y + 1
W1:
		ror
		ror accu + 2
		ror accu + 1
		ror accu
		dex
		bne	L1

		lsr x
		bcc W2

		tay
		sec
		lda accu + 2
		sbc y
		sta accu + 2
		tya
		sbc y + 1

		sec
W2:
		ror
		ror accu + 2
		ror accu + 1
		ror accu
		sta accu + 3

	}
}

inline int lmul12f4s(int x, int y)
{
	return (int)(lmul16s(x, y) >> 4);
}

inline int lmul8f8s(int x, int y)
{
	return (int)(lmul16s(x, y) >> 8);
}

int lmul4f12s(int x, int y)
{
	__asm
	{
		bit y + 1
		bpl W0

		sec
		lda #0
		sbc y
		sta y
		lda #0
		sbc y + 1
		sta y + 1

		sec
		lda #0
		sbc x
		sta x
		lda #0
		sbc x + 1
		sta x + 1
W0:
		ldx	#15
		lda #0
		sta	accu + 1

L1:		lsr	x + 1
		ror	x
		bcc	W1
		tay
		clc
		lda	accu + 1
		adc	y
		sta accu + 1
		tya
		adc y + 1
W1:
		ror
		ror accu + 1
		ror accu
		dex
		bne	L1

		lsr x
		bcc W2

		tay
		sec
		lda accu + 1
		sbc y
		sta accu + 1
		tya
		sbc y + 1

		sec
W2:
		ror
		ror accu + 1
		ror accu

		lsr
		ror accu + 1
		ror accu

		lsr
		ror accu + 1
		ror accu

		lsr
		ror accu + 1
		ror accu

		lsr
		ror accu + 1
		ror accu
	}
}

unsigned ldiv16u(unsigned long x, unsigned y)
{
	__asm
	{
			lda	#0
			sta accu
			sta accu + 1

			ldx #17
	L1:
			sec
			lda x + 2
			sbc y
			tay
			lda x + 3
			sbc y + 1
			bcc	W1
			sta x + 3
			sty x + 2
	W1:
			rol accu
			rol accu + 1

			asl x
			rol x + 1
			rol x + 2
			rol x + 3

			dex
			beq E1
			bcc L1

			lda x + 2
			sbc y
			sta x + 2
			lda x + 3
			sbc y + 1
			sta x + 3
			sec
			bcs W1
	E1:
	}
}

int ldiv16s(long x, int y)
{
	if (x < 0)
	{
		if (y < 0)
			return ldiv16u(-x, - y);
		else
			return -ldiv16u(-x, y);
	}
	else if (y < 0)
		return -ldiv16u(x, -y);
	else
		return ldiv16u(x, y);
}

inline int ldiv12f4s(int x, int y)
{
	return (int)(ldiv16s((long)x << 4, y));
}

inline int ldiv8f8s(int x, int y)
{
	return (int)(ldiv16s((long)x << 8, y));
}

inline int ldiv4f12s(int x, int y)
{
	return (int)(ldiv16s((long)x << 12, y));
}

unsigned lmuldiv16u(unsigned a, unsigned b, unsigned c)
{
	__asm
	{
			lda	#0
			sta	__tmp + 0
			sta	__tmp + 1
			sta	__tmp + 2
			sta	__tmp + 3

			ldx	#16
	L1:		lsr	a + 1
			ror	a
			bcc	W1
			clc
			lda	__tmp + 2
			adc	b
			sta	__tmp + 2
			lda	__tmp + 3
			adc b + 1
			sta	__tmp + 3
	W1:
			ror __tmp + 3
			ror __tmp + 2
			ror __tmp + 1
			ror __tmp
			dex
			bne	L1

			lda	#0
			sta accu
			sta accu + 1

			ldx #17
	L2:
			sec
			lda __tmp + 2
			sbc c
			tay
			lda __tmp + 3
			sbc c + 1
			bcc	W2
			sta __tmp + 3
			sty __tmp + 2
	W2:
			rol accu
			rol accu + 1

			asl __tmp
			rol __tmp + 1
			rol __tmp + 2
			rol __tmp + 3

			dex
			beq E2
			bcc L2

			lda __tmp + 2
			sbc c
			sta __tmp + 2
			lda __tmp + 3
			sbc c + 1
			sta __tmp + 3
			sec
			bcs W2
	E2:

	}

}

int lmuldiv16s(int a, int b, int c)
{
	bool	sign = false;
	if (a < 0)
	{
		a = -a;
		sign = !sign;
	}
	if (b < 0)
	{
		b = -b;
		sign = !sign;
	}
	if (c < 0)
	{
		c = -c;
		sign = !sign;
	}

	long	v = lmuldiv16u(a, b, c);

	if (sign)
		return -v;
	else
		return v;
}
