#include <c64/memmap.h>
#include <c64/charwin.h>
#include <c64/cia.h>
#include <c64/vic.h>
#include <c64/kernalio.h>
#include <oscar.h>

// Common memory area for all overlays

#pragma region( main, 0x0900, 0x8000, , , { code, data, bss, heap, stack } )

// Section and region for first overlay bank

#pragma overlay( ovl1, 1, lzo )
#pragma section( bcode1, 0 )
#pragma section( bdata1, 0 )
#pragma region(bank1, 0x8000, 0xc000, , 1, { bcode1, bdata1 } )

// Section and region for second overlay bank

#pragma overlay( ovl2, 2, lzo )
#pragma section( bcode2, 0 )
#pragma section( bdata2, 0 )
#pragma region(bank2, 0x8000, 0xc000, , 2, { bcode2, bdata2 } )

#pragma overlay( ovl3, 3, lzo )
#pragma section( bcode3, 0 )
#pragma section( bdata3, 0 )
#pragma region(bank3, 0x8000, 0xc000, , 3, { bcode3, bdata3 } )

#pragma overlay( ovl4, 4, lzo )
#pragma section( bcode4, 0 )
#pragma section( bdata4, 0 )
#pragma region(bank4, 0x8000, 0xc000, , 4, { bcode4, bdata4 } )

#pragma overlay( ovl5, 5, lzo )
#pragma section( bcode5, 0 )
#pragma section( bdata5, 0 )
#pragma region(bank5, 0x8000, 0xc000, , 5, { bcode5, bdata5 } )

#pragma overlay( ovl6, 6, lzo )
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
	cwin_put_string(&cw, p"This is first overlay", 7);
	cwin_cursor_newline(&cw);
}

// Now switch code generation to bank 2

#pragma code ( bcode2 )
#pragma data ( bdata2 )

void print2(void)
{
	cwin_put_string(&cw, p"This is second overlay", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode3 )
#pragma data ( bdata3 )

void print3(void)
{
	cwin_put_string(&cw, p"This is third overlay", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode4 )
#pragma data ( bdata4 )

void print4(void)
{
	cwin_put_string(&cw, p"This is fourth overlay", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode5 )
#pragma data ( bdata5 )

void print5(void)
{
	cwin_put_string(&cw, p"This is fifth overlay", 7);
	cwin_cursor_newline(&cw);
}

#pragma code ( bcode6 )
#pragma data ( bdata6 )

void print6(void)
{
	cwin_put_string(&cw, p"This is sixth overlay", 7);
	cwin_cursor_newline(&cw);
}

// Switching code generation back to shared section

#pragma code ( code )
#pragma data ( data )

// Load an overlay section into memory

void load(const char * fname)
{
	krnio_setnam(fname);
	if (krnio_open(2, 8, 2))
	{
		if (krnio_chkin(2))
		{
			char lo = krnio_chrin();
			char hi = krnio_chrin();

			char * dp = (char *)((hi << 8) | lo);
			krnio_clrchn();
			krnio_read_lzo(2, dp);
		}
		krnio_close(2);
	}
}

int main(void)
{
	// Kernal memory only
	mmap_set(MMAP_NO_BASIC);

	// Init VIC
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1800);

	// Prepare output window
	cwin_init(&cw, (char *)0x0400, 0, 0, 40, 25);
	cwin_clear(&cw);

	// Call function in overlay 1
	load(P"OVL1");
	print1();

	// Call function in overlay 2
	load(P"OVL2");
	print2();

	load(P"OVL3");
	print3();

	load(P"OVL4");
	print4();

	load(P"OVL5");
	print5();

	load(P"OVL6");
	print6();

	mmap_set(MMAP_ROM);

	return 0;
}
