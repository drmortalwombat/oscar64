#include "conio.h"

static IOCharMap		giocharmap = IOCHM_ASCII;

#if defined(__C128__)
#pragma code(lowcode)
__asm bsout
{	
		ldx #0
		stx 0xff00
		jsr 0xffd2
		sta 0xff01
}
__asm bsin
{
		lda #0
		sta 0xff00
		jsr 0xffe4
		sta 0xff01	
}
__asm bsplot
{	
		lda #0
		sta 0xff00
		jsr 0xfff0
		sta 0xff01
}
__asm bsinit
{	
		lda #0
		sta 0xff00
		jsr 0xff81
		sta 0xff01
}
__asm dswap
{	
		sta 0xff00
		jsr 0xff5f
		sta 0xff01
}
#pragma code(code)
#elif defined(__C128B__) || defined(__C128E__)
#define dswap 	0xff5f
#define bsout	0xffd2
#define bsin	0xffe4
#define bsplot	0xfff0
#define bsinit	0xff81
#elif defined(__PLUS4__)
#pragma code(lowcode)
__asm bsout
{	
		sta 0xff3e
		jsr 0xffd2
		sta 0xff3f
}
__asm bsin
{
		sta 0xff3e
		jsr 0xffe4
		sta 0xff3f
}
__asm bsplot
{	
		sta 0xff3e
		jsr 0xfff0
		sta 0xff3f
}
__asm bsinit
{	
		sta 0xff3e
		jsr 0xff81
		sta 0xff3f
}
#pragma code(code)
#elif defined(__ATARI__)
__asm bsout
{
		tax
		lda	0xe407
		pha
		lda 0xe406
		pha
		txa
}

__asm bsin
{
		lda	0xe405
		pha
		lda 0xe404
		pha
}

__asm bsplot
{

}
__asm bsinit
{

}
#else
#define bsout	0xffd2
#define bsin	0xffe4
#define bsplot	0xfff0
#define bsinit	0xff81
#endif

#if defined(__C128__) || defined(__C128B__) || defined(__C128E__)
void dispmode40col(void)
{
	if (*(volatile char *)0xd7 >= 128)
	{
		__asm
		{		
			jsr dswap
		}
	}
}

void dispmode80col(void)
{
	if (*(volatile char *)0xd7 < 128)
	{
		__asm
		{		
			jsr dswap
		}
	}
}
#endif


void iocharmap(IOCharMap chmap)
{
	giocharmap = chmap;	
#if !defined(__ATARI__)
	if (chmap == IOCHM_PETSCII_1)
		putch(128 + 14);
	else if (chmap == IOCHM_PETSCII_2)
		putch(14);
#endif
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

#if defined(__CBMPET__)
		cmp	#97
		bcs	w4
		cmp #91
		bcs	w3
	w2:
		eor	#$a0
	w4:
		eor #$20

#else
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
#endif
		cpx #IOCHM_PETSCII_2
		beq	w3
		and #$df
	w3:
		jmp	bsout	
}

__asm getpch
{
		jsr	bsin

		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp	#13
		bne	w1
		lda #10
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #219
		bcs w3
		cmp #65
		bcc w3

		cmp #193
		bcc w4
		eor #$a0
	w4:
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


char kbhit(void)
{
	__asm
	{
		lda $c6
		sta	accu
	}
}

char getche(void)
{
	__asm
	{
	L1:
		jsr	getpch
		cmp	#0
		beq	L1

		sta	accu
		jsr putpch
	}

}

char getch(void)
{
	__asm
	{
	L1:
		jsr	getpch
		cmp	#0
		beq	L1

		sta	accu
	}
}

char getchx(void)
{
	__asm
	{
		jsr	getpch
		sta	accu
	}
}

void putch(char c)
{
	__asm {
		lda	c
		jsr	bsout
	}
}

void clrscr(void)
{
	__asm
	{
		jsr	bsinit
	}
}

void textcursor(bool show)
{
	*(char *)0xcc = show ? 0 : 1;
}

void gotoxy(char cx, char cy)
{
	__asm
	{
		ldx	cy
		ldy	cx
		clc
		jsr bsplot
	}	
}

void textcolor(char c)
{
	__asm
	{
		lda	c
		sta $0286
	}
}

char wherex(void)
{
	__asm
	{
		lda $d3
		sta	accu
	}
}

char wherey(void)
{
	__asm
	{
		lda $d6
		sta	accu
	}
}
