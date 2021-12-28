#include <stdio.h>
#include <c64/kernalio.h>

int main(void)
{
	krnio_setnam("@0:CHARS,P,W");	
	if (krnio_open(2, 9, 2))
	{
		for(char i=0; i<128; i++)
			krnio_putch(2, i);

		krnio_close(2);
	}

	return 0;
}
