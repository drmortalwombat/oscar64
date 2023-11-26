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

inline unsigned long lsqr4f12s(int x)
{
	if (x < 0) x = -x;
	return lmul16u(x, x);
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
			lda	sign
			beq	E1

			sec
			lda	#0
			sbc accu
			sta accu
			lda #0
			sbc accu + 1
			sta accu + 1
	E1:	
	}
}


unsigned lmuldiv16by8(unsigned a, char b, char c)
{
	__asm {

		lda	#0
		sta	accu + 0
		sta	accu + 1
		sta	accu + 2
		sta	accu + 3

		lda b
		beq z1

		lda	c
	l1:
		asl
		bcs	e1
		cmp b
		bcs e2

		asl	a + 0
		rol a + 1
		jmp l1
	e2:
		clc
	e1:
		ror
		sta c

		ldx #16
	l2:
		lda b
		sec
		sbc c
		bcc	w1
		sta b

		clc
		lda accu + 2
		adc a
		sta accu + 2
		lda accu + 3
		adc a + 1
		sta accu + 3
		bcc * + 8
		inc accu + 0
		bne * + 4
		inc accu + 1

	w1:
		asl	accu + 2
		rol	accu + 3
		rol accu + 0
		rol accu + 1

		asl	b
		bcc w2
		lda b
		sbc c
		sta b

		clc
		lda accu + 2
		adc a
		sta accu + 2
		lda accu + 3
		adc a + 1
		sta accu + 3
		bcc * + 8
		inc accu + 0
		bne * + 4
		inc accu + 1

	w2:
		dex
		bne l2
	z1:		
	}
}

unsigned lmuldiv8by8(char a, char b, char c)
{
	__asm {

		ldy	#0
		sty	accu + 0
		sty	accu + 1
		sty	accu + 2

		lda a
		beq	z1
		lda c
		beq	z1

		ldx #8
		lda	b
		beq	z1

		cmp c
		bcc w0
	l1:
		lsr
		ror accu + 1
		inx
		cmp c
		bcs l1
		bcc	wa

	l2:
		asl accu + 2
		rol accu + 0
	wa:
		rol accu + 1
	w0:
		rol

		bcc w2
		sbc c
		bcc w3
	w2:
		tay
		sec
		sbc	c
		bcc	w1
	w3:		
		tay

		clc
		lda accu + 2
		adc a
		sta accu + 2
		bcc w1
		inc accu + 0
		bne w1
		inc accu + 1
	w1:
		tya

		dex
		bne l2
	z1:		
	}
}

int lmuldiv16sby8(int a, char b, char c)
{
	if (a < 0)
		return -(int)lmuldiv16by8(-a, b, c);
	else
		return lmuldiv16by8(a, b, c);
}

unsigned usqrt(unsigned n)
{
    unsigned p, q, r, h;

    p = 0;
    r = n;

#assign q 0x4000
#repeat
    {
        h = p | q;
        p >>= 1;
        if (r >= h)
        {
            p |= q;
            r -= h;
        } 
    }
#assign q q >> 2
#until q == 0
#undef q

    return p;
}

unsigned long lmul16f16(unsigned long x, unsigned long y)
{
	unsigned long hh = lmul16u(x >> 16, y >> 16);
	unsigned long lh = lmul16u(x, y >> 16);
	unsigned long hl = lmul16u(x >> 16, y);
	unsigned long ll = lmul16u(x, y);

	if (ll & 0x8000)
		lh++;
	ll >>= 16;
	ll |= hh << 16;
	ll += lh;
	ll += hl;

	return ll;
}

__native long lmul16f16s(long x, long y)
{
	unsigned lox = x;
	int hix = x >> 16;
	unsigned loy = y;
	int hiy = y >> 16;

	long	r = (long)(hix * hiy) << 16;

	if (lox)
	{
		r += lmul16u(lox, hiy);
		if (hiy < 0)
			r -= (unsigned long)lox << 16;
	}
	if (loy)
	{
		r += lmul16u(loy, hix);
		if (hix < 0)
			r -= (unsigned long)loy << 16;
	}
	if (lox && loy)
	{
		r += lmul16u(lox, loy) >> 16;
	}

	return r;
}

__native unsigned long ldiv16f16(unsigned long x, unsigned long y)
{
	unsigned long k = x >> 16, d = 0;	
	x <<= 16;

	for(char i=0; i<32; i++)
	{
		d <<= 1;
		k <<= 1;
		k |= (x >> 31);
		x <<= 1;
		if (k >= y)
		{
			k -= y;
			d |= 1;
		}
	}

	return d;
}

__native long ldiv16f16s(long x, long y)
{
	bool	sign = false;	
	if (x < 0)
	{
		x = -x;
		sign = true;
	}
	if (y < 0)
	{
		y = -y;
		sign = !sign;
	}

	x = ldiv16f16(x, y);
	if (sign)
		return -x;
	else
		return x;
}
