#include <string.h>
#include <stdio.h>
#include <c64/asm6502.h>

// Invoke resident section with "SYS 49152" from basic

// not much space, so we go with a smaller stack size

#pragma stacksize(256)

// shrink size of startup section

#pragma section(startup, 0);
#pragma region(startup, 0x0801, 0x0860, , , { startup } )

// section for code copy

#pragma section(rcode, 0)
#pragma region(rcode, 0x0860, 0x0900, , , { rcode } )

// main section to stay resident, save three bytes at the
// beginning to have space for an entry jump

#pragma region(main, 0x0903, 0x1900, , , {code, data, bss, heap, stack}, 0xc003 )

// resident entry routine

void tsr(void)
{
	// Initialize stack pointer

	__asm {
		lda #$ff
		sta __sp
		lda	#$cf
		sta	__sp +1
	}

	// do something useless

	printf(p"Hello World\n");

	// and done
}
	
// Now the copy code section

#pragma code(rcode)

int main(void)
{
	// source and target address to copy, no memcpy as it is itself
	// not yet copied

	char * dp = (char *)0xc000;
	char * sp = (char *)0x0903;

	// A jmp to the code at the absolute address 0xc000 / 49152

	dp += asm_ab(dp, ASM_JMP, (unsigned)tsr);

	// then the code 

	for(unsigned i=0; i<0xffd; i++)
		*dp++ = *sp++;

	// now we are done

	return 0;
}
