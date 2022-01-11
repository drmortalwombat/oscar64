#include <c64/memmap.h>
#include <c64/vic.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <math.h>
#include <gfx/vector3d.h>
#include <gfx/mcbitmap.h>
#include <stdio.h>

#pragma region(main, 0x0a00, 0xc800, , , {code, data, bss, heap, stack} )


#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)
#define Hires	((char *)0xe000)

Bitmap		Screen = {
	Hires, nullptr, 40, 25, 320
};

ClipRect	SRect = {
	0, 0, 320, 200
}


Matrix4	wmat, pmat, tmat, rmat;
Vector3	vlight;


void init(void)
{
	mmap_set(MMAP_NO_BASIC);

	vic_setmode(VICM_HIRES_MC, Color1, Hires);

	vic.color_back = VCOL_DARK_GREY;
	vic.color_border = VCOL_DARK_GREY;

	mmap_trampoline();
	mmap_set(MMAP_NO_ROM);

	memset(Color1, 0xcf, 1000);
	memset(Color2, 0x01, 1000);
	memset(Hires, 0, 8000);
}

void restore(void)
{
	vic_setmode(VICM_TEXT, (char *)0x0400, (char *)0x1000);
	mmap_set(MMAP_ROM);
}

struct Point
{
	int x, y;	
};

#define HALF	15
#define FULL	(HALF + HALF)
#define SIZE	(FULL + 1)
#define QFULL	(FULL * FULL)

Vector3	v[SIZE][SIZE];
Point	p[SIZE][SIZE];
float	z[SIZE][SIZE];

struct Surf
{
	float	z;
	char	x, y;
}	surfs[QFULL];



void qsort(Surf * n, int s)
{
	if (s > 1)
	{
		Surf	pn = n[0];
		int 	pi = 0;
		for(int i=1; i<s; i++)
		{
			if (n[i].z > pn.z)
			{
				n[pi] = n[i];
				pi++;
				n[i] = n[pi]
			}
		}
		n[pi] = pn;

		qsort(n, pi);
		qsort(n + pi + 1, s - pi - 1);
	}
}


int main(void)
{
	init();

	bm_put_string(&Screen, &SRect, 0, 0, "Preparing function", BLTOP_COPY);

	mat4_ident(&wmat);
	mat4_make_perspective(&pmat, 0.5 * PI, 1.0, 0.0, 200.0);
	
	bm_put

	for(int ix=0; ix<SIZE; ix++)
	{
		for(int iy=0; iy<SIZE; iy++)
		{
			float	x = (ix - HALF) * (1.0 / HALF), y = (HALF - iy) * (1.0 / HALF);

			float	r = sqrt(x * x + y * y);
			float	f = - cos(r * 16) * exp(- 2 * r);

			vec3_set(&(v[iy][ix]), x, f * 0.5, y);
		}
	}

	bm_put_string(&Screen, &SRect, 0, 8, "Projecting vertices", BLTOP_COPY);

	vec3_set(&vlight, 2.0, -2.0, -1.0);
	vec3_norm(&vlight);

	mat4_scale(&wmat, 18);

	mat4_set_rotate_x(&rmat, -0.98);
	mat4_set_rotate_y(&tmat, 0.3);
	mat4_rmmul(&rmat, &tmat);

	mat4_rmmul(&rmat, &wmat);
	rmat.m[14] += 20.0;

	tmat = pmat;
	mat4_mmul(&tmat, &rmat);

	for(int ix=0; ix<SIZE; ix++)
	{
		for(int iy=0; iy<SIZE; iy++)
		{	
			Vector3 vp;

			vec3_project(&vp, &tmat, &(v[iy][ix]));

			p[iy][ix].x = vp.v[0] * 140 + 160;
			p[iy][ix].y = vp.v[1] * 140 + 80;
			z[iy][ix] = vp.v[2];
		}
	}


	bm_put_string(&Screen, &SRect, 0, 16, "Sorting surfaces", BLTOP_COPY);

	for(int iy=0; iy<FULL; iy++)
	{
		for(int ix=0; ix<FULL; ix++)
		{
			surfs[FULL * iy + ix].z = 
				z[iy + 0][ix + 0] +
				z[iy + 0][ix + 1] +
				z[iy + 1][ix + 0] +
				z[iy + 1][ix + 1];
			surfs[FULL * iy + ix].x = ix;
			surfs[FULL * iy + ix].y = iy;
		}
	}

	qsort(surfs, QFULL);


	bm_put_string(&Screen, &SRect, 0, 24, "Drawing surfaces", BLTOP_COPY);

	for(int i=0; i< QFULL; i++)
	{
		char ix = surfs[i].x, iy = surfs[i].y;

		Vector3	d0, d1, n;				

		vec3_diff(&d0, &(v[iy + 0][ix + 0]), &(v[iy + 1][ix + 1]));
		vec3_diff(&d1, &(v[iy + 1][ix + 0]), &(v[iy + 0][ix + 1]));

		vec3_xmul(&n, &d0, &d1);
		vec3_norm(&n);

		float	f = vec3_vmul(&vlight, &n);
		int		c = 0;
		char	patt = 3;
		if (f > 0)
		{
			c = 1 + (int)(f * 6);
			if (c > 4)
				patt = 0;
		}

		bmmc_quad_fill(&Screen, &SRect, 
			p[iy + 0][ix + 0].x, p[iy + 0][ix + 0].y, 
			p[iy + 0][ix + 1].x, p[iy + 0][ix + 1].y, 
			p[iy + 1][ix + 1].x, p[iy + 1][ix + 1].y, 
			p[iy + 1][ix + 0].x, p[iy + 1][ix + 0].y, 
			MixedColors[c >> 1][(c + 1) >> 1]);

#if 0
		bmmc_line(&Screen, &SRect, 
			p[iy + 0][ix + 0].x, p[iy + 0][ix + 0].y, 
			p[iy + 0][ix + 1].x, p[iy + 0][ix + 1].y, patt);

		bmmc_line(&Screen, &SRect, 
			p[iy + 1][ix + 0].x, p[iy + 1][ix + 0].y, 
			p[iy + 1][ix + 1].x, p[iy + 1][ix + 1].y, patt);

		bmmc_line(&Screen, &SRect, 
			p[iy + 0][ix + 0].x, p[iy + 0][ix + 0].y, 
			p[iy + 1][ix + 0].x, p[iy + 1][ix + 0].y, patt);

		bmmc_line(&Screen, &SRect, 
			p[iy + 0][ix + 1].x, p[iy + 0][ix + 1].y, 
			p[iy + 1][ix + 1].x, p[iy + 1][ix + 1].y, patt);
#endif
	}

	mmap_set(MMAP_NO_BASIC);
	getch();

	restore();

	return 0;
}