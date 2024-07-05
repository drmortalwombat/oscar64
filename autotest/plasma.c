#include <stdlib.h>

static const char sintab[] = {
	128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 239, 240, 241, 243, 244, 245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255, 
	255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246, 245, 244, 243, 241, 240, 239, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220, 218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179, 177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 140, 137, 134, 131, 
	128, 125, 122, 119, 116, 112, 109, 106, 103, 100, 97, 94, 91, 88, 85, 82, 79, 77, 74, 71, 68, 65, 63, 60, 57, 55, 52, 50, 47, 45, 43, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 21, 19, 17, 16, 15, 13, 12, 11, 10, 8, 7, 6, 6, 5, 4, 3, 3, 2, 2, 2, 1, 1, 1, 
	1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 6, 6, 7, 8, 10, 11, 12, 13, 15, 16, 17, 19, 21, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 43, 45, 47, 50, 52, 55, 57, 60, 63, 65, 68, 71, 74, 77, 79, 82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 116, 119, 122, 125
};

char Screen0[1024], Screen1[1024];

char colormap0[256], colormap1[256];


char colors0[] = {0, 6, 14, 1, 13, 5, 0};
char colors1[] = {0, 9, 7, 1, 15, 12, 0};

unsigned c1A, c1B, c2A, c2B, c3A, c3B;
int d1A, d1B, d2A, d2B, d3A, d3B;

void inithires(void)
{
	for(int i=0; i<256; i++)
	{
		colormap0[i] = colors0[i / 37];
		colormap1[i] = colors1[i / 37] << 4;
	}
}

inline void doplasma(char * scrn) 
{
    char xbuf0[40], xbuf1[40];
    char ybuf0[25], ybuf1[25];

    char c2a = c2A >> 8;
    char c2b = c2B >> 8;
    char c1a = c1A >> 8;
    char c1b = c1B >> 8;

    for (char i = 0; i < 25; i++) {
        ybuf0[i] = sintab[(c1a + c2a) & 0xff] + sintab[c1b];
        c1a += 13;
        c1b -= 5;
    }

    for (char i = 0; i < 40; i++) {
        xbuf0[i] = sintab[(c2a + c1b) & 0xff] + sintab[c2b];
        c2a += 11;
        c2b -= 7;
    }

    c2a = c2B >> 8;
    c2b = c3A >> 8;
    c1a = c1B >> 8;
    c1b = c3B >> 8;

    for (char i = 0; i < 25; i++) {
        ybuf1[i] = sintab[(c1b + c2a) & 0xff] + sintab[c1a];
        c1a += 4;
        c1b -= 6;
    }

    for (char i = 0; i < 40; i++) {
        xbuf1[i] = sintab[(c2b + c1a) & 0xff] + sintab[c2a];
        c2a += 7;
        c2b -= 9;
    }

    #pragma unroll(full)
	for (char k=0; k<5; k++)    
	{
    	char tbuf0[5], tbuf1[5];
	    #pragma unroll(full)
	    for (char i = 0; i < 4; i++)
	    {
	    	tbuf0[i] = ybuf0[5 * k + i + 1] - ybuf0[5 * k + i];
	    	tbuf1[i] = ybuf1[5 * k + i + 1] - ybuf1[5 * k + i];
	    }

	    for (signed char i = 39; i >= 0; i--) 
	    {
	    	char t = xbuf0[i] + ybuf0[5 * k];
	    	char u = xbuf1[i] + ybuf1[5 * k];

		    #pragma unroll(full)
		    for (char j = 0; j < 5; j++) 
		    {
	            scrn[40 * j + 200 * k + i] = colormap0[u] | colormap1[u];
		    	t += tbuf0[j];
		    	u += tbuf1[j];		    	
	        }
	    }
	}

    c1A += 8 * ((int)sintab[d1A] - 128);
    c1B += 16 * ((int)sintab[d1B] - 128);
    c2A += 8 * ((int)sintab[d2A] - 128);
    c2B += 16 * ((int)sintab[d2B] - 128);
    c3A += 6 * ((int)sintab[d3A] - 128);
    c3B += 12 * ((int)sintab[d3B] - 128);

    d1A += 3;
    d1B += rand() & 3;
    d2A += 5;
    d2B += rand() & 3;
    d3A += 2;
    d3B += rand() & 3;
}

void doplasma0(void)
{
	doplasma(Screen0);
}

void doplasma1(void)
{
	doplasma(Screen1);
}

unsigned checksum(const char * scr)
{
	unsigned	s = 0x1234;
	for(int i=0; i<1024; i++)
	{
		unsigned	m = s & 1;
		s >>= 1;
		if (m)
			s ^= 0x2152;
		s ^= scr[i];
	}
	return s;
}
int main(void)
{
	inithires();

	doplasma0();
	doplasma1();
	doplasma0();
	doplasma1();
	doplasma0();
	doplasma1();

	return	checksum(Screen0) + checksum(Screen1) - 16337;
}
