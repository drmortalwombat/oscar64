#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define MAP_WIDTH 10
#define MAP_HEIGHT 2

#define TITLE_TILE_WIDTH            4
#define TITLE_TILE_HEIGHT           4

#define PTR_SCREEN              ((char *)0xc000)
#define PTR_BUFFER              ((char *)0xc400)
#define PTR_COLOR               ((char *)0xd800)
#define PTR_FONTCHARSET         ((char *)0xd800)

const char TitleMap[1024] = {
#for(i, 1024) i * 17,
};

const char TitleTiles[4096] = {
#for(i, 4096) i * 31,
};

// Custom screen address
extern char* const Screen = PTR_SCREEN;

// Color mem address
extern char* const Color = PTR_COLOR;

void RenderLogo(char screenY)
{    
	char * sp = Screen;
	char * cp = Color;
	const char * mp = TitleMap;

	for(char ty=0; ty < MAP_HEIGHT; ty++)
    {
        for(char tx=0; tx< MAP_WIDTH; tx++)
        {
            char ti = mp[tx];
            const char* tp = TitleTiles + (TITLE_TILE_WIDTH * TITLE_TILE_HEIGHT) * ti;

            for(char y=0; y<TITLE_TILE_HEIGHT; y++)
            {
                for(char x=0; x<TITLE_TILE_WIDTH; x++)
                {	
                    char c = tp[TITLE_TILE_WIDTH * y + x];
                    sp[40 * (y + screenY) + x] = c;
                    cp[40 * (y + screenY) + x] = 1;
                }    
            }
            sp += TITLE_TILE_WIDTH;
            cp += TITLE_TILE_WIDTH;
        }
        sp += 120;
        cp += 120;
	    mp += MAP_WIDTH;
    }
}

void VerifyLogo(char screenY)
{
	for(char dy=0; dy<MAP_HEIGHT * TITLE_TILE_HEIGHT; dy++)
	{
		for(char dx=0; dx<MAP_WIDTH * TITLE_TILE_WIDTH; dx++)
		{
			char ty = dy / TITLE_TILE_HEIGHT, iy = dy % TITLE_TILE_HEIGHT;
			char tx = dx / TITLE_TILE_WIDTH, ix = dx % TITLE_TILE_WIDTH;

			int si = TitleMap[MAP_WIDTH * ty + tx] * TITLE_TILE_WIDTH * TITLE_TILE_HEIGHT + TITLE_TILE_WIDTH * iy + ix;
			int di = 40 * (dy + screenY) + dx;

			assert(Screen[di] == TitleTiles[si]);
		}
	}
}

int main(void)
{
    RenderLogo(1);
    VerifyLogo(1);
    return 0;
}
