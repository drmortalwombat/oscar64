#include <c64/charwin.h>
#include <assert.h>
#include <stdio.h>

int main(void)
{
	CharWin	cw;

	cwin_init(&cw, (char *)0x0400, 2, 2, 32, 20);

	cwin_clear(&cw);

	for(int y=0; y<20; y++)
	{
		for(int x=0; x<32; x++)
		{
			assert(cwin_getat_char_raw(&cw, x, y) == ' ');
		}
	}

	for(int y=0; y<20; y++)
	{
		for(int x=0; x<32; x++)
		{
			cwin_putat_char(&cw, x, y, p'a' - 1 + x, 7);
		}		
	}


	for(int y=0; y<20; y++)
	{
		for(int x=0; x<32; x++)
		{
			assert(cwin_getat_char(&cw, x, y) == p'a' -1 + x);
		}		
	}


	for(int y=0; y<20; y++)
	{
		for(int x=0; x<32; x++)
		{
			cwin_putat_char(&cw, x, y, p'A' - 1 +  x, 8);
		}		
	}

	for(int y=0; y<20; y++)
	{
		for(int x=0; x<32; x++)
		{
			cwin_putat_char_raw(&cw, x, y, x + 32 * y, 8);
		}		
	}

	for(int y=0; y<8; y++)
	{
		for(int x=0; x<32; x++)
		{
			assert(cwin_getat_char_raw(&cw, x, y) == x + 32 * y);
		}		
	}

	return 0;
}
