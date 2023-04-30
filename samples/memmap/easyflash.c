#include <c64/memmap.h>
#include <c64/charwin.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/easyflash.h>

// Shared code/data region, copied from easyflash bank 0 to ram during startup

#pragma region( main, 0x0900, 0x8000, , , { code, data, bss, heap, stack } )

// Section and region for first easyflash bank

#pragma section( bcode1, 0 )
#pragma section( bdata1, 0 )
#pragma region(bank1, 0x8000, 0xc000, , 1, { bcode1, bdata1 } )

// Section and region for second easyflash bank

#pragma section( bcode2, 0 )
#pragma section( bdata2, 0 )
#pragma region(bank2, 0x8000, 0xc000, , 2, { bcode2, bdata2 } )

#pragma section( bcode3, 0 )
#pragma section( bdata3, 0 )
#pragma region(bank3, 0x8000, 0xc000, , 3, { bcode3, bdata3 } )

#pragma section( bcode4, 0 )
#pragma section( bdata4, 0 )
#pragma region(bank4, 0x8000, 0xc000, , 4, { bcode4, bdata4 } )

#pragma section( bcode5, 0 )
#pragma section( bdata5, 0 )
#pragma region(bank5, 0x8000, 0xc000, , 5, { bcode5, bdata5 } )

#pragma section( bcode6, 0 )
#pragma section( bdata6, 0 )
#pragma region(bank6, 0x8000, 0xc000, , 6, { bcode6, bdata6 } )

// Charwin in shared memory section

CharWin	cw;

// Now switch code generation to bank 1

#pragma code ( bcode1 )
#pragma data ( bdata1 )

// Print into shared charwin

void print1(void)
{
	cwin_put_string(&cw, p"This is first bank", 7);
	cwin_cursor_newline(&cw);
}

// Now switch code generation to bank 2

#pragma code ( bcode2 )
#pragma data ( bdata2 )

void print2(void)
{
	cwin_put_string(&cw, p"This is second bank", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode3 )
#pragma data ( bdata3 )

void print3(void)
{
	cwin_put_string(&cw, p"This is third bank", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode4 )
#pragma data ( bdata4 )

void print4(void)
{
	cwin_put_string(&cw, p"This is fourth bank", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode5 )
#pragma data ( bdata5 )

void print5(void)
{
	cwin_put_string(&cw, p"This is fifth bank", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode6 )
#pragma data ( bdata6 )

void print6(void)
{
	cwin_put_string(&cw, p"This is sixth bank", 7);
	cwin_cursor_newline(&cw);
}

// Switching code generation back to shared section

#pragma code ( code )
#pragma data ( data )


// Function for indirect cross bank call
void fcall(char bank, void (* func)())
{
	eflash.bank = bank;
	func();
}

// Macro for indirect cross bank call
#define FCALL(f) fcall(__bankof(f), f)

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

	// Switch easyflash ROM region to bank 1
	eflash.bank = 1;

	// Call function in bank 1
	print1();

	// Switch easyflash ROM region to bank 2
	eflash.bank = 2;

	// Call function in bank 2
	print2();

	eflash.bank = 3;
	print3();

	// Get bank of function using __bankof operator
	eflash.bank = __bankof print4;
	print4();

	// Indirect call
	fcall(__bankof print5, print5);

	// Macro call
	FCALL(print6);

	// Loop forever
	while (true)
		;

	return 0;
}
