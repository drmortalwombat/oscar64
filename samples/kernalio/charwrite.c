#include <stdio.h>
#include <c64/kernalio.h>

int main(void)
{
	// Set name for file and open it with replace on drive 9
	krnio_setnam("@0:CHARS,P,W");	
	if (krnio_open(2, 9, 2))
	{
		// Write 128 bytes to the file, it would be more efficient
		// to set the output channel with krnio_chkout() for the file and 
		// write the bytes using krnio_chrout()
		
		for(char i=0; i<128; i++)
			krnio_putch(2, i);

		// Close the file again
		krnio_close(2);
	}

	return 0;
}
