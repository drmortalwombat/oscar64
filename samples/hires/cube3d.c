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

struct FVector3
{
	int v[3];
};

struct FMatrix3
{
	int	m[9];
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

FVector3	corners[8];

static const int FMOne = 1 << 12;

void fmat3_ident(FMatrix3 * m)
{
	m->m[0] = FMOne;
	m->m[1] = 0;
	m->m[2] = 0;

	m->m[3] = 0;
	m->m[4] = FMOne;
	m->m[5] = 0;

	m->m[6] = 0;
	m->m[7] = 0;
	m->m[8] = FMOne;
}

void fmat3_mmul(FMatrix3 * md, const FMatrix3 * ms)
{
	for(char i=0; i<3; i++)
	{
		char	j = 3 * i;

		int	m0 = lmul4f12s(md->m[i + 0], ms->m[0]) + lmul4f12s(md->m[i + 3], ms->m[1]) + lmul4f12s(md->m[i + 6], ms->m[2]);
		int	m3 = lmul4f12s(md->m[i + 0], ms->m[3]) + lmul4f12s(md->m[i + 3], ms->m[4]) + lmul4f12s(md->m[i + 6], ms->m[5]);
		int	m6 = lmul4f12s(md->m[i + 0], ms->m[6]) + lmul4f12s(md->m[i + 3], ms->m[7]) + lmul4f12s(md->m[i + 6], ms->m[8]);

		md->m[i + 0] = m0; md->m[i + 3] = m3; md->m[i + 6] = m6;
	}
}

void fmat3_rmmul(FMatrix3 * md, const FMatrix3 * ms)
{
	for(char i=0; i<9; i+=3)
	{
		int	m0 = lmul4f12s(md->m[i + 0], ms->m[0]) + lmul4f12s(md->m[i + 1], ms->m[3]) + lmul4f12s(md->m[i + 2], ms->m[6]);
		int	m1 = lmul4f12s(md->m[i + 0], ms->m[1]) + lmul4f12s(md->m[i + 1], ms->m[4]) + lmul4f12s(md->m[i + 2], ms->m[7]);
		int	m2 = lmul4f12s(md->m[i + 0], ms->m[2]) + lmul4f12s(md->m[i + 1], ms->m[5]) + lmul4f12s(md->m[i + 2], ms->m[8]);

		md->m[i + 0] = m0; md->m[i + 1] = m1; md->m[i + 2] = m2;
	}
}

void fmat3_set_rotate_x(FMatrix3 * m, float a)
{
	int	c = (int)(FMOne * cos(a));
	int	s = (int)(FMOne * sin(a));
	m->m[0] = FMOne; m->m[3] = 0; m->m[6] = 0;
	m->m[1] = 0; m->m[4] = c; m->m[7] = s;
	m->m[2] = 0; m->m[5] =-s; m->m[8] = c;
}

void fmat3_set_rotate_y(FMatrix3 * m, float a)
{
	int	c = (int)(FMOne * cos(a));
	int	s = (int)(FMOne * sin(a));
	m->m[0] = c; m->m[3] = 0; m->m[6] = s;
	m->m[1] = 0; m->m[4] = FMOne; m->m[7] = 0;
	m->m[2] =-s; m->m[5] = 0; m->m[8] = c;
}


void fmat3_set_rotate_z(FMatrix3 * m, float a)
{
	int	c = (int)(FMOne * cos(a));
	int	s = (int)(FMOne * sin(a));
	m->m[0] = c; m->m[3] =-s; m->m[6] = 0;
	m->m[1] = s; m->m[4] = c; m->m[7] = 0;
	m->m[2] = 0; m->m[5] = 0; m->m[8] = FMOne;
}

void fvec3_mmul(FVector3 * vd, const FMatrix3 * m, const FVector3 * vs)
{
	FVector3	vt;
	for(char i=0; i<3; i++)
		vt.v[i] = lmul4f12s(m->m[i], vs->v[0]) + lmul4f12s(m->m[3 + i], vs->v[1]) + lmul4f12s(m->m[6 + i], vs->v[2]);
	*vd = vt;
}


FMatrix3	rmat, tmat;

int main(void)
{
	init();

	for(char i=0; i<8; i++)
	{
		corners[i].v[0] = (i & 1) ? -FMOne : FMOne;
		corners[i].v[1] = (i & 2) ? -FMOne : FMOne;
		corners[i].v[2] = (i & 4) ? -FMOne : FMOne;
	}
 	
	for(int k=0; k<100; k++)
	{
		fmat3_set_rotate_x(&rmat, 0.1 * k);
		fmat3_set_rotate_y(&tmat, 0.06 * k);
		fmat3_mmul(&rmat, &tmat);

	 	for(char i=0; i<8; i++)
		{
			FVector3	vd;

			fvec3_mmul(&vd, &rmat, corners + i);

			tcorners[i].x = lmuldiv16s(vd.v[0], 140, vd.v[2] + 4 * FMOne) + 160;
			tcorners[i].y = lmuldiv16s(vd.v[1], 140, vd.v[2] + 4 * FMOne) + 100;
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