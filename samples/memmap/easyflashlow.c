#include <c64/memmap.h>
#include <c64/charwin.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <string.h>

#pragma section( startup, 0 )

#pragma region( startup, 0x0100, 0x0200, , , { startup } )

#pragma region( main, 0x0400, 0x8000, , , { code, data, bss, heap, stack } )

CharWin	cw;

int main(void)
{
	// Copy Char ROM
	mmap_set(MMAP_ALL_ROM);

	memcpy((char *)0xd000, (char *)0xd000, 0x1000);	

	// Enable ROM
	mmap_set(MMAP_ROM);

	// Init CIAs (no kernal rom was executed so far)
	cia_init();

	// Init VIC
	vic_setmode(VICM_TEXT, (char *)0xc000, (char *)0xd800);

	// Prepare output window
	cwin_init(&cw, (char *)0xc000, 0, 0, 40, 25);
	cwin_clear(&cw);

	cwin_put_string(&cw, p"Hello World", 7);

	while (true) ; 
}
