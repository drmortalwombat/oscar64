#include <c64/vic.h>
#include <c64/rasterirq.h>
#include <string.h>

const char * Text =
	S"LABORUM RERUM QUO. QUASI IN, SEQUI, TENETUR VOLUPTATEM RERUM "
	S"PORRO NON ET MAIORES ALIAS ODIO EST EOS. MAGNAM APERIAM CUM ET "
	S"ESSE TEMPORE ITAQUE TEMPORA VOLUPTAS ET IPSAM IPSAM EARUM. ID "
	S"SUSCIPIT QUIA RERUM REPREHENDERIT ERROR ET UT. DOLOR ID "
	S"CORPORIS, EOS? UNDE VERO ISTE QUIA? EAQUE EAQUE. IN. AUT ID "
	S"EXPEDITA ILLUM MOLESTIAS, ";

RIRQCode	scroll, bottom;

int main(void)
{
	rirq_init(true);

	rirq_build(&scroll, 1);
	rirq_write(&scroll, 0, &vic.ctrl2, 0);
	rirq_set(0, 50 + 24 * 8, &scroll);

	rirq_build(&bottom, 1);
	rirq_write(&bottom, 0, &vic.ctrl2, VIC_CTRL2_CSEL);
	rirq_set(1, 250, &bottom);

	rirq_sort();

	rirq_start();

	int	x = 0;
	for(;;)
	{
		rirq_wait();
		rirq_data(&scroll, 0, 7 - (x & 7));
		if ((x & 7) == 0)
			memcpy((char *)0x0400 + 40 * 24, Text + ((x >> 3) & 255), 40);
		x++;
	}

	return 0;
}
