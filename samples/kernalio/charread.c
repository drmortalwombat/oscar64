#include <stdio.h>
#include <c64/kernalio.h>

int main(void)
{
	// Set name for file and open it on drive 9
	krnio_setnam("@0:CHARS,P,R");	
	if (krnio_open(2, 9, 2))
	{
		// Read bytes until failure
		int ch, k = 0;
		while ((ch = krnio_getch(2)) >= 0)
		{
			// Print the value of the byte
			printf("%d : %d\n", k, ch)
			k++;

			// Exit the loop if this was the last byte of the file
			if (ch & 0x100)
				break;
		}

		// Close the file
		krnio_close(2);
	}

	return 0;
}
