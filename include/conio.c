#include "conio.h"

static IOCharMap		giocharmap = IOCHM_ASCII;

void iocharmap(IOCharMap chmap)
{
	giocharmap = chmap;
	if (chmap == IOCHM_PETSCII_1)
		putchar(128 + 14);
	else if (chmap == IOCHM_PETSCII_2)
		putchar(14);
}

__asm putpch
{
		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp #10
		bne	w1
		lda #13
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #65
		bcc w3
		cmp	#123
		bcs	w3
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
		cpx #IOCHM_PETSCII_2
		beq	w3
		and #$df
	w3:
		jmp	0xffd2	
}

__asm getpch
{
		jsr	0xffe4

		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp	#13
		bne	w1
		lda #10
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #65
		bcc w3
		cmp	#123
		bcs	w3
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
	w3:
}


int kbhit(void)
{
	__asm
	{
		lda $c6
		sta	accu
		lda	#0
		sta	accu + 1		
	}
}

int getche(void)
{
	__asm
	{
	L1:
		jsr	getpch
		cmp	#0
		beq	L1

		sta	accu
		jsr putpch
		lda	#0
		sta	accu + 1		
	}

}

int getch(void)
{
	__asm
	{
	L1:
		jsr	getpch
		cmp	#0
		beq	L1

		sta	accu
		lda	#0
		sta	accu + 1		
	}
}

void putch(int c)
{
	__asm {
		ldy	#c
		lda	(fp), y
		jsr	0xffd2
	}
}

void clrscr(void)
{
	__asm
	{
		jsr	$ff5b
	}
}

void textcursor(bool show)
{
	*(char *)0xcc = show ? 0 : 1;
}

void gotoxy(int cx, int cy)
{
	__asm
	{
		ldy	cy
		ldx	cx
		clc
		jsr $fff0
	}	
}

void textcolor(int c)
{
	__asm
	{
		lda	c
		sta $0286
	}
}

int wherex(void)
{
	__asm
	{
		lda $d3
		sta	accu
		lda	#0
		sta	accu + 1		
	}
}

int wherey(void)
{
	__asm
	{
		lda $d6
		sta	accu
		lda	#0
		sta	accu + 1		
	}
}
