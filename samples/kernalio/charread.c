#include <stdio.h>
#include <c64/kernalio.h>

int main(void)
{
	krnio_setnam("@0:CHARS,P,R");	
	if (krnio_open(2, 9, 2))
	{
		int ch, k = 0;
		while ((ch = krnio_getch(2)) >= 0)
		{
			printf("%d : %d\n", k, ch)
			k++;
			if (ch & 0x100)
				break;
		}

		krnio_close(2);
	}

	return 0;
}
