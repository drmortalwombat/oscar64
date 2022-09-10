#include <c64/memmap.h>
#include <c64/charwin.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/easyflash.h>
#include <string.h>

// Shared code/data region, copied from easyflash bank 0 to ram during startup

#pragma region( main, 0x0900, 0x8000, , , { code, data, bss, heap, stack } )

// Section and region for first easyflash bank, code is compiled for a
// target address of 0x7000 but placed into the bank at 0x8000
// The data section is first to ensure the jump table is at the start
// address

#pragma section( bcode1, 0 )
#pragma section( bdata1, 0 )
#pragma region(bank1, 0x8000, 0x9000, , 1, { bdata1, bcode1  }, 0x7000 )

// Section and region for second easyflash bank

#pragma section( bcode2, 0 )
#pragma section( bdata2, 0 )
#pragma region(bank2, 0x8000, 0x9000, , 2, { bdata2, bcode2  }, 0x7000 )

#pragma section( bcode3, 0 )
#pragma section( bdata3, 0 )
#pragma region(bank3, 0x8000, 0x9000, , 3, { bdata3, bcode3  }, 0x7000 )

#pragma section( bcode4, 0 )
#pragma section( bdata4, 0 )
#pragma region(bank4, 0x8000, 0x9000, , 4, { bdata4, bcode4  }, 0x7000 )

#pragma section( bcode5, 0 )
#pragma section( bdata5, 0 )
#pragma region(bank5, 0x8000, 0x9000, , 5, { bdata5, bcode5  }, 0x7000 )

#pragma section( bcode6, 0 )
#pragma section( bdata6, 0 )
#pragma region(bank6, 0x8000, 0x9000, , 6, { bdata6, bcode6  } , 0x7000 )

// Charwin in shared memory section

CharWin	cw;

struct EntryTable
{
	void (*fhello)(void);
	void (*fdone)(void);
};	

// Now switch code generation to bank 1

#pragma code ( bcode1 )
#pragma data ( bdata1 )

// Print into shared charwin

void print1(void)
{
	cwin_put_string(&cw, p"This is first bank", 7);
}

void done1(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry1 = {
	.fhello = &print1,
	.fdone = &done1
};

// make sure the function is referenced
#pragma reference(entry1)

// Now switch code generation to bank 2

#pragma code ( bcode2 )
#pragma data ( bdata2 )

void print2(void)
{
	cwin_put_string(&cw, p"This is second bank", 7);
}

void done2(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry2 = {
	.fhello = &print2,
	.fdone = &done2
};

// make sure the function is referenced
#pragma reference(entry2)

#pragma code ( bcode3 )
#pragma data ( bdata3 )

void print3(void)
{
	cwin_put_string(&cw, p"This is third bank", 7);
}

void done3(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry3 = {
	.fhello = &print3,
	.fdone = &done3
};

#pragma reference(entry3)

#pragma code ( bcode4 )
#pragma data ( bdata4 )

void print4(void)
{
	cwin_put_string(&cw, p"This is fourth bank", 7);
}

void done4(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry4 = {
	.fhello = &print4,
	.fdone = &done4
};

#pragma reference(entry4)

#pragma code ( bcode5 )
#pragma data ( bdata5 )

void print5(void)
{
	cwin_put_string(&cw, p"This is fifth bank", 7);
}

void done5(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry5 = {
	.fhello = &print5,
	.fdone = &done5
};

#pragma reference(entry5)

#pragma code ( bcode6 )
#pragma data ( bdata6 )

void print6(void)
{
	cwin_put_string(&cw, p"This is sixth bank", 7);
}

void done6(void)
{
	cwin_cursor_newline(&cw);
}

const EntryTable	entry6 = {
	.fhello = &print6,
	.fdone = &done6
};

#pragma reference(entry6)

// Switching code generation back to shared section

#pragma code ( code )
#pragma data ( data )

// Copy the data of the rom bank, and call the function
// with the jump table

void callbank(char bank)
{
	eflash.bank = bank;
	memcpy((char *)0x7000, (char *)0x8000, 0x1000);

	// call function at start of copied section
	((EntryTable *)0x7000)->fhello();
	((EntryTable *)0x7000)->fdone();
}

int main(void)
{
	// Enable ROM
	mmap_set(MMAP_ROM);

	// Init CIAs (no kernal rom was executed so far)
	cia_init();

	// Init VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

	// Prepare output window
	cwin_init(&cw, (char *)0x0400, 0, 0, 40, 25);
	cwin_clear(&cw);


	// Call function in bank 1
	callbank(1);
	
	// Switch easyflash ROM region to bank 2
	callbank(2);

	callbank(3);
	callbank(4);
	callbank(5);
	callbank(6);

	// Loop forever
	while (true)
		;

	return 0;
}
