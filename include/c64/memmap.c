#include "memmap.h"

__asm DoneTrampoline
{
	stx $01
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

	lda #>DoneTrampoline
	pha
	lda #<DoneTrampoline
	pha
	tsx
	lda $0105, x
	pha
	ldx $01
	lda #$36
	sta $01
	jmp ($fffe)
}

__asm NMITrampoline
{
	pha
	txa
	pha

	lda #>DoneTrampoline
	pha
	lda #<DoneTrampoline
	pha
	tsx
	lda $0105, x
	pha
	ldx $01
	lda #$36
	sta $01
	jmp ($fffa)
}

void mmap_trampoline(void)
{
	*((void **)0xfffa) = NMITrampoline;	
	*((void **)0xfffe) = IRQTrampoline;
}

#pragma native(mmap_trampoline)

char mmap_set(char pla)
{
	char ppla = *((char *)0x01);
	*((volatile char *)0x01) = pla;
	return ppla;
}
