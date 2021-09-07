#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

char s[25][41] = {"Q QQ   QQQQQ        QQQQQQQQQQQQQ",
				  "              QQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQ",
				  "                       QQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "",
				  "",
				  "",
				  "                QQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ",
				  "QQQQQQQQQ",
				  "",
				  "",
				  "",
				  "",
				  "",
				  "",
				  "",
				  "",
				  "                          QQQQQQQQQQQQQQ"};

int main() 
{
	int i=0, sum = 0;
	for (unsigned char i = 0; i < 25; i++)
	{
		int j = 0;
		while (s[i][j])
		{
			sum += s[i][j] & 3;
			j++;
		}
	}
	
	assert(sum == 391);

	return 0;
}