#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <conio.h>
#include <string.h>


char spdata[] = {
#embed "../resources/friendlybear.bin"	
};

RIRQCode	open, bottom;

int main(void)
{
	rirq_init(true);

	rirq_build(&open, 1);
	rirq_write(&open, 0, &vic.ctrl1, VIC_CTRL1_DEN | 3);
	rirq_set(0, 50 + 200 - 3, &open);

	rirq_build(&bottom, 1);
	rirq_write(&bottom, 0, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3 );
	rirq_set(1, 50, &bottom);

	rirq_sort();

	rirq_start();

	memcpy((char *)0x0380, spdata, 128);

	*(char *)(0x7f8) = 0x03c0 / 64;
	*(char *)(0x7f9) = 0x0380 / 64;
	*(char *)(0x7fa) = 0x03c0 / 64;
	*(char *)(0x7fb) = 0x0380 / 64;
	*(char *)(0x7fc) = 0x03c0 / 64;
	*(char *)(0x7fd) = 0x0380 / 64;
	*(char *)(0x7fe) = 0x03c0 / 64;
	*(char *)(0x7ff) = 0x0380 / 64;

	vic.spr_enable = 0b11111111;
    vic.spr_multi = 0b10101010;
    vic.spr_color[0] = VCOL_BLACK;
    vic.spr_color[1] = VCOL_ORANGE;
    vic.spr_color[2] = VCOL_BLACK;
    vic.spr_color[3] = VCOL_ORANGE;
    vic.spr_color[4] = VCOL_BLACK;
    vic.spr_color[5] = VCOL_ORANGE;
    vic.spr_color[6] = VCOL_BLACK;
    vic.spr_color[7] = VCOL_ORANGE;
    vic.spr_mcolor0 = VCOL_BROWN;
    vic.spr_mcolor1 = VCOL_WHITE;

    for(;;)
    {
    	for(int i=0; i<255; i++)
    	{
			vic_sprxy(0, 100, 1 * i); vic_sprxy(1, 100, 1 * i);
			vic_sprxy(2, 140, 2 * i); vic_sprxy(3, 140, 2 * i);
			vic_sprxy(4, 180, 3 * i); vic_sprxy(5, 180, 3 * i);
			vic_sprxy(6, 220, 4 * i); vic_sprxy(7, 220, 4 * i);

    		rirq_wait();
    	}
    }

	return 0;
}
