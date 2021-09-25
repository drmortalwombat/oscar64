// short index ops
#include <stdio.h>

char a[20];

int main(void)
{
	for(char i=0; i<20; i++)
		a[i] = i;
	char x = 0;
	for(char i=0; i<20; i++)
		x += a[i];
		
	printf("Sum %d\n", x);
	return 0;
}
