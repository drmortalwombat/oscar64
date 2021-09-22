#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char	text[14];
	
	for(char i=0; i<12; i++)
	{
		switch (i)
		{
		case 0:
			text[i] = 'H';
			break;
		case 1:
			text[i] = 'E';
			break;
		case 2:
		case 3:
		case 9:
			text[i] = 'L';
			break;
		case 4:
		case 7:
			text[i] = 'O';
			break;
		case 5:
			text[i] = ' ';
			break;
		case 6:
			text[i] = 'W';
			break;
		case 8:
			text[i] = 'R';
			break;
		case 10:
			text[i] = 'D';
			break;
		default:
			text[i] = 0;
		}
	}

	printf("<%s>\n", text);
	
	return 0;
}
