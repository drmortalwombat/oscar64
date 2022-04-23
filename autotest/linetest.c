#include <gfx/bitmap.h>
#include <assert.h>


char 	* const	Hires = (char *)0x4000;
Bitmap			Screen;
ClipRect		cr = {0, 0, 320, 200};



int main(void)
{
	bm_init(&Screen, Hires, 40, 25);	

	bmu_rect_clear(&Screen, 0, 0, 320, 200);

	bm_line(&Screen, &cr, 0, 0, 199, 199, 0xff, LINOP_SET);

	for(int i=0; i<200; i++)
	{
		assert(Hires[(i & 7) + 320 * (i >> 3) + (i & ~7)] == 0x80 >> (i & 7));
	}

	return 0;
}
