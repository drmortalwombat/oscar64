#include "conio.h"

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
		jsr	$ffe4
		cmp	#0
		beq	L1

		sta	accu
		jsr $ffd2
		lda	#0
		sta	accu + 1		
	}

}

int getch(void)
{
	__asm
	{
	L1:
		jsr	$ffe4
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

void gotoxy(int x, int y)
{
	__asm
	{
		ldy	#y
		lda	(fp), y
		tax
		ldy	#x
		lda	(fp), y
		tay
		clc
		jsr $fff0
	}	
}

void textcolor(int c)
{
	__asm
	{
		ldy	#c
		lda	(fp), y
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
