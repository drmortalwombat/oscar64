#include <c64/vic.h>
#include <stdlib.h>
#include <stdio.h>


// space for our custom charset from c800

#pragma section( charset, 0)

#pragma region( charset, 0xc800, 0xd000, , , {charset} )

#pragma data(charset)

char charset[2048] = {
	#embed "../resources/charset.bin"
}

#pragma data(data)

#define Screen	((char *)0xc000)


int main(void)
{
	// map the vic to the new charset

	vic_setmode(VICM_TEXT, Screen, charset)

	for(int i=0; i<1000; i++)
		Screen[i] = (char)i;

    return 0;
}

