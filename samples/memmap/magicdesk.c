#include <c64/memmap.h>
#include <c64/charwin.h>
#include <c64/cia.h>
#include <c64/vic.h>

// Set rom region end at 0x9f80 to leave some space for copy code
#pragma region(rom, 0x8080, 0x9f80, , 0, { code, data } )

// We do not want to pollute main RAM region with our data
#pragma region(main, 0xc000, 0xd000, , , { bss, stack, heap })
#pragma stacksize( 256 )

// Region and code for a cross bank copy routine
#pragma section( ccode, 0 )
#pragma region( crom, 0x9f80, 0xa000, , 0, { ccode }, 0x0380 )

CharWin	cw;

// Copy code from any bank to RAM, returns to bank 0, this code is 
// copied from bank 0 to RAM at 0x0380 at program start

#pragma code ( ccode )

__export void ccopy(char bank, char * dst, const char * src, unsigned n)
{
	*((volatile char *)0xde00) = bank;
	while (n)
	{
		*dst++ = *src++;
		n--;
	}
	*((volatile char *)0xde00) = 0;
}

// Region of code to be executed from RAM after copied from 2nd ROM
// bank

#pragma section( bcode1, 0 )
#pragma section( bdata1, 0 )
#pragma region(bank1, 0x8000, 0xa000, , 1, { bcode1, bdata1 }, 0x2000 )

#pragma code ( bcode1 )
#pragma data ( bdata1 )

// Print into shared charwin

void print1(void)
{
	cwin_put_string(&cw, p"This is first bank", 1);
	cwin_cursor_newline(&cw);
}

// Back to main sections in bank 0

#pragma code ( code )
#pragma data ( data )


int main(void)
{
	// Enable ROM
	mmap_set(MMAP_ROM);

	// Init CIAs (no kernal rom was executed so far)
	cia_init();

	// Copy ccopy code to RAM
	for(char i=0; i<128; i++)
		((char *)0x0380)[i] = ((char *)0x9f80)[i];

	// Init VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

	// Prepare output window
	cwin_init(&cw, (char *)0x0400, 0, 0, 40, 25);
	cwin_clear(&cw);

	// Write first line
	cwin_put_string(&cw, p"Hello World", 7);
	cwin_cursor_newline(&cw);

	// Copy bank 1 to RAM
	ccopy(1, (char *)0x2000, (char *)0x8000, 0x2000);

	// Call function in copy
	print1();

	// Third line
	cwin_put_string(&cw, p"Final words", 14);
	cwin_cursor_newline(&cw);

	while (true) ;

	return 0;
}
