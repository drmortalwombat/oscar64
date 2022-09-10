#include <c64/vic.h>
#include <stdlib.h>
#include <stdio.h>

// make space until 0x2000 for code and data

#pragma region( lower, 0x0a00, 0x2000, , , {code, data} )

// then space for our custom charset

#pragma section( charset, 0)

#pragma region( charset, 0x2000, 0x2800, , , {charset} )

// everything beyond will be code, data, bss and heap to the end

#pragma region( main, 0x2800, 0xa000, , , {code, data, bss, heap, stack} )


#pragma data(charset)

char charset[2048] = {
	#embed "../resources/charset.bin"
};

#pragma data(data)

int main(void)
{
	// map the vic to the new charset

	vic_setmode(VICM_TEXT, (char *)0x0400, charset);

	for(int i=0; i<10; i++)
		printf(p"%D Hello World\n", i);	

    return 0;
}

