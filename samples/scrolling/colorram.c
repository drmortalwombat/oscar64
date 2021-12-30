#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdlib.h>

#define screen ((byte *)0x0400)
#define color ((byte *)0xd800)
#define sline(x, y) (screen + 40 * (y) + (x))
#define cline(x, y) (color + 40 * (y) + (x))

char rbuff[25], cbuff[25];

#define SPLIT1	8
#define SPLIT2	16

void scrollLeft0(void)
{
	for(char x=0; x<39; x++)
	{
#assign y 0		
#repeat
		sline(0, y)[x] = sline(1, y)[x];
		cline(0, y)[x] = cline(1, y)[x];
#assign y y + 1
#until y == SPLIT1
	}
#assign y 0		
#repeat
	sline(0, y)[39] = rbuff[y];
	cline(0, y)[39] = cbuff[y];
#assign y y + 1
#until y == SPLIT1
}

void scrollLeft1(void)
{
	for(char x=0; x<39; x++)
	{
#assign y SPLIT1
#repeat
		sline(0, y)[x] = sline(1, y)[x];
		cline(0, y)[x] = cline(1, y)[x];
#assign y y + 1
#until y == SPLIT2
	}
#assign y SPLIT1	
#repeat
	sline(0, y)[39] = rbuff[y];
	cline(0, y)[39] = cbuff[y];
#assign y y + 1
#until y == SPLIT2
	for(char x=0; x<39; x++)
	{
#assign y SPLIT2
#repeat
		sline(0, y)[x] = sline(1, y)[x];
		cline(0, y)[x] = cline(1, y)[x];
#assign y y + 1
#until y == 25
	}
#assign y SPLIT2	
#repeat
	sline(0, y)[39] = rbuff[y];
	cline(0, y)[39] = cbuff[y];
#assign y y + 1
#until y == 25
}



void prepcol(void)
{
	for(char i=0; i<25; i++)
	{
		unsigned r = rand();
		cbuff[i] = r & 15;
		rbuff[i] = (r & 16) ? 102 : 160;
	}
}

int main(void)
{
	memset(screen, 0x20, 1000);
	memset(color, 7, 1000);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	char	x = 0;

	for(;;)
	{
		x = (x + 1) & 7;

		if (x == 0)
		{
			vic_waitLine(50 + 8 * SPLIT1);
			scrollLeft0();
		}

		vic_waitBottom();

		vic.ctrl2 = (7 - x) & 7;

		if (x == 0)
		{
			scrollLeft1();
		}
		else 
		{
			if (x == 4)
				prepcol();
			vic_waitTop();
		}
	}

	return 0;

}
