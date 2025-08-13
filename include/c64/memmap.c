// This code is meant to keep the C64 running while the program it is part of banks
// out the BASIC or Kernal ROM to have RAM available. If an IRQ or NMI occurs during 
// that time and a ROM routine is called that isn't there, the C64 will crash.
// This code replaces the IRQ and NMI handling code to bank the BASIC and Kernal ROMs
// back in, call the original handling routines, and then restore the situation as it
// was when the interrupt happened.

#include "memmap.h"

__asm DoneTrampoline
{
	stx $01				// The ROM code at jmp ($fffa) has saved our X value 
						//  so we can restore it to $01. Our banks our back to whatever it was
	pla					//  now we pull X and A and restore them
	tax
	pla
	rti					//  RTI can now pull the original status byte and return address and 
						//  return to the original code.
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
							//  NMI just happend, so stack contains ($01ff for example):
							//  $01ff High byte of return address
							//  $01fe Low byte of return address
							//  $01fd status flag byte

	pha						//  $01fc save A on the stack
	txa
	pha						//  $01fb save X on the stack

	lda #>DoneTrampoline	//  $01fa save the high byte of DoneTrampoline()
	pha
	lda #<DoneTrampoline	//  $01f9 save the low byte of DoneTrampoline()
	pha
	tsx						//  transfer the SP ($f8) to X
	lda $0105, x			//  $0105 + $f8 = $01fd (we have virtually shifted the end of stack
							//  			 to $0105 to get to the original status flag)
	pha						//  and we push it again
	ldx $01					//  Now we save the current $01 value so we can restore it later
	lda #$36				//  set $01 to its default value (bank ROMs back in)
	sta $01
	jmp ($fffa)				//  call the original handler (we are looking at ROM now, not RAM)
							//  this routine saves A, X and Y and ends in an RTI 
							//  that will pop SP and the DoneTrampoline() address and jump to it
}

void mmap_trampoline(void)
{
	// This is to set the IRQ and NMI handler hooks to our own code.
	// But note, that his is written to and saved in RAM under ROM at $fffa/$fffb and $fffe/$ffff
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
