#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/kernalio.h>
#include <stdio.h>

#define Screen	((char *)0xcc00)
#define Charset	((char *)0xc000)

int main(void)
{
	// Set name for file and open it on drive 9
	krnio_setnam("CHARSET,P,R");	
	if (krnio_open(2, 8, 2))
	{
		// Read the content of the file into the charset buffer,
		// decompressing on the fly
		krnio_read_lzo(2, Charset);

		// Close the file
		krnio_close(2);
	}

	// Change display address to new screen and charset
	
	vic_setmode(VICM_TEXT, Screen, Charset)

	for(int i=0; i<1000; i++)
		Screen[i] = (char)i;

	// wait for keypress

	getchar();

	// restore VIC

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000)

	// restore basic ROM
	mmap_set(MMAP_ROM);

    return 0;
}


