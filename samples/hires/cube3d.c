#include <c64/memmap.h>
#include <c64/vic.h>
#include <gfx/bitmap.h>
#include <string.h>
#include <conio.h>
#include <stdio.h>
#include <gfx/vector3d.h>
#include <math.h>
#include <fixmath.h>

char * const Color = (char *)0xd000;
char * const Hires = (char *)0xe000;

Bitmap		Screen;

void init(void)
{
	mmap_trampoline();
	mmap_set(MMAP_RAM);

	memset(Color, 0x01, 1000);
	memset(Hires, 0x00, 8000);

	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES, Color, Hires);

	vic.color_border = VCOL_WHITE;

	bm_init(&Screen, Hires, 40, 25);	
}

void done(void)
{
	mmap_set(MMAP_ROM);

//	getch();

	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
}

ClipRect	cr = {0, 0, 320, 200};


struct Point
{
	int x, y;	
};


Point	tcorners[8], pcorners[8];

void drawCube(void)
{
	for(char i=0; i<8; i++)
	{
		if (!(i & 1))
			bm_line(&Screen, &cr, tcorners[i].x, tcorners[i].y, tcorners[i | 1].x, tcorners[i | 1].y, 0xff, LINOP_OR);
		if (!(i & 2))
			bm_line(&Screen, &cr, tcorners[i].x, tcorners[i].y, tcorners[i | 2].x, tcorners[i | 2].y, 0xff, LINOP_OR);
		if (!(i & 4))
			bm_line(&Screen, &cr, tcorners[i].x, tcorners[i].y, tcorners[i | 4].x, tcorners[i | 4].y, 0xff, LINOP_OR);
		pcorners[i] = tcorners[i];
	}
}

void hideCube(void)
{
	for(char i=0; i<8; i++)
	{
		if (!(i & 1))
			bm_line(&Screen, &cr, pcorners[i].x, pcorners[i].y, pcorners[i | 1].x, pcorners[i | 1].y, 0xff, LINOP_AND);
		if (!(i & 2))
			bm_line(&Screen, &cr, pcorners[i].x, pcorners[i].y, pcorners[i | 2].x, pcorners[i | 2].y, 0xff, LINOP_AND);
		if (!(i & 4))
			bm_line(&Screen, &cr, pcorners[i].x, pcorners[i].y, pcorners[i | 4].x, pcorners[i | 4].y, 0xff, LINOP_AND);
	}
}

#if 1

F12Vector3	corners[8];



F12Matrix3	rmat, tmat;

int main(void)
{
	init();

	for(char i=0; i<8; i++)
	{
		corners[i].v[0] = (i & 1) ? -FIX12_ONE : FIX12_ONE;
		corners[i].v[1] = (i & 2) ? -FIX12_ONE : FIX12_ONE;
		corners[i].v[2] = (i & 4) ? -FIX12_ONE : FIX12_ONE;
	}
 	
	for(int k=0; k<100; k++)
	{
		f12mat3_set_rotate_x(&rmat, 0.1 * k);
		f12mat3_set_rotate_y(&tmat, 0.06 * k);
		f12mat3_mmul(&rmat, &tmat);

	 	for(char i=0; i<8; i++)
		{
			F12Vector3	vd;

			f12vec3_mmul(&vd, &rmat, corners + i);

			tcorners[i].x = lmuldiv16s(vd.v[0], 140, vd.v[2] + 4 * FIX12_ONE) + 160;
			tcorners[i].y = lmuldiv16s(vd.v[1], 140, vd.v[2] + 4 * FIX12_ONE) + 100;
		}

		hideCube();
		drawCube();

	}

	done();

	return 0;
}

#else

Matrix4	wmat, pmat, tmat, rmat;
Vector3	corners[8];

int main(void)
{
	init();

	mat4_ident(&wmat);
	mat4_make_perspective(&pmat, 0.5 * PI, 1.0, 0.0, 200.0);

	mat4_scale(&wmat, 1);

	for(char i=0; i<8; i++)
	{
		vec3_set(corners + i, 
			(i & 1) ? -1.0 : 1.0, 
			(i & 2) ? -1.0 : 1.0, 
			(i & 4) ? -1.0 : 1.0);
	}
 	
	for(int k=0; k<100; k++)
	{
		mat4_set_rotate_x(&rmat, 0.1 * k);
		mat4_set_rotate_y(&tmat, 0.06 * k);
		mat4_rmmul(&rmat, &tmat);

		mat4_rmmul(&rmat, &wmat);
		rmat.m[14] += 4.0;

		tmat = pmat;
		mat4_mmul(&tmat, &rmat);

	 	for(char i=0; i<8; i++)
		{
			Vector3	vd;
			vec3_project(&vd, &rmat, corners + i);
			tcorners[i].x = (int)(vd.v[0] * 100) + 160;
			tcorners[i].y = (int)(vd.v[1] * 100) + 100;
		}

		hideCube();
		drawCube();
	}

	done();

	return 0;
}

#endif