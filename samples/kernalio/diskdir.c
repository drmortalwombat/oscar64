#include <stdio.h>
#include <c64/kernalio.h>

int main(void)
{
	// Set name for directory

	krnio_setnam("$");	

	// Open #2 on drive 9 (or 8)

	if (krnio_open(2, 9, 0))
	{
		// Switch input to file #2

		if (krnio_chkin(2))
		{
			// Skip BASIC load address

			krnio_chrin();
			krnio_chrin();
			
			// Loop while we have more lines

			int ch;
			while((ch = krnio_chrin()) > 0)
			{
				unsigned line;
				char	buff[40];			
			
				// Skip second basic link byte	
				krnio_chrin();
				
				// Read line number (size in blocks)
				ch = krnio_chrin();
				line = ch;
				ch = krnio_chrin();
				line += 256 * ch;
				
				// Read file name, reading till end of basic line
				int n = 0;
				while ((ch = krnio_chrin()) > 0)
					buff[n++] = ch;
				buff[n] = 0;
				
				// Print size and name

				printf("%u %s\n", line, buff);			
			}
			
			// Reset channels

			krnio_clrchn();
		}
		
		// Close file #2
		
		krnio_close(2);
	}
	else
		printf("FAIL OPEN %d\n", krnio_status());
	
	return 0;
}
