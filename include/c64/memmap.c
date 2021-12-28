#include "memmap.h"

char PLAShadow;

__asm DoneTrampoline
{
	lda PLAShadow
	sta $01
	pla
	tax
	pla
	rti
}

__asm IRQTrampoline
{
	pha
	txa
	pha
	lda #$36
	sta $01

	lda #>DoneTrampoline
	pha
	lda #<DoneTrampoline
	pha
	tsx
	lda $0105, x
	pha
	jmp ($fffa)
}

__asm NMITrampoline
{
	pha
	txa
	pha
	lda #$36
	sta $01

	lda #>DoneTrampoline
	pha
	lda #<DoneTrampoline
	pha
	tsx
	lda $0105, x
	pha
	jmp ($fffe)
}

void mmap_trampoline(void)
{
	*((void **)0xfffa) = IRQTrampoline;
	*((void **)0xfffe) = NMITrampoline;	
}

#pragma native(mmap_trampoline)

void mmap_set(char pla)
{
	PLAShadow = pla;
	*((char *)0x01) = pla;
}
