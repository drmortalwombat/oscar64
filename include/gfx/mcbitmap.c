#include "mcbitmap.h"
#include <c64/asm6502.h>

static char andmask[8] = {0x3f, 0x3f, 0xcf, 0xcf, 0xf3, 0xf3, 0xfc, 0xfc};
static char ormask[8] = {0xc0, 0xc0, 0x30, 0x30, 0x0c, 0x0c, 0x03, 0x03};
static char cbytes[4] = {0x00, 0x55, 0xaa, 0xff};

char MixedColors[4][4][8] = {
	{
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44},
		{0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88},
		{0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc},
	},
	{
		{0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11},
		{0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
		{0x66, 0x99, 0x66, 0x99, 0x66, 0x99, 0x66, 0x99},
		{0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd},
	},
	{
		{0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22},
		{0x99, 0x66, 0x99, 0x66, 0x99, 0x66, 0x99, 0x66},
		{0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa},
		{0xbb, 0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb, 0xee},
	},
	{
		{0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33, 0xcc, 0x33},
		{0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77},
		{0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb},
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	},
};

void bmmc_put(const Bitmap * bm, int x, int y, char c)
{
	char	*	dp = bm->data + bm->cwidth * (y & ~7) + ((x & ~7) | (y & 7));
	char		pat = cbytes[c & 3];

	*dp = ((*dp ^ pat) & andmask[x & 7]) ^ pat;
}

char bmmc_get(const Bitmap * bm, int x, int y)
{
	char * dp = bm->data + bm->cwidth * (y & ~7) + (x & ~7) + (y & 7);

	return (*dp >> (6 - (x & 6))) & 3;
}


void bmmcu_circle(const Bitmap * bm, int x, int y, char r, char color)
{
	char	*	lpt = bm->data + bm->cwidth * (y & ~7) + (y & 7);
	char	*	lpb = lpt;
	int			stride = 8 * bm->cwidth - 8;
	char		pat = ~cbytes[color & 3];

	char rx = r, ry = 0;
	int	d = r / 2;
	char * dp;

	while (rx > 0)
	{
		dp = lpt + ((x + rx) & ~7);
		*dp = ((*dp ^ pat) | ormask[(x + rx) & 7]) ^ pat;
		dp = lpt + ((x - rx) & ~7);
		*dp = ((*dp ^ pat) | ormask[(x - rx) & 7]) ^ pat;

		dp = lpb + ((x + rx) & ~7);
		*dp = ((*dp ^ pat) | ormask[(x + rx) & 7]) ^ pat;
		dp = lpb + ((x - rx) & ~7);
		*dp = ((*dp ^ pat) | ormask[(x - rx) & 7]) ^ pat;

		if (d >= 0)
		{
			ry++;
			d -= ry;
			lpb ++;
			if (!((int)lpb & 7))
				lpb += stride;
			if (!((int)lpt & 7))
				lpt -= stride;
			lpt--;
		}
		if (d < 0)
		{
			rx--;
			d += rx;
		}
	}

	dp = lpt + (x & ~7);
	*dp = ((*dp ^ pat) | ormask[x & 7]) ^ pat;
	dp = lpb + (x & ~7);
	*dp = ((*dp ^ pat) | ormask[x & 7]) ^ pat;
}

void bmmc_circle2(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, char color)
{
	char	*	lpt = bm->data + bm->cwidth * (y & ~7) + (y & 7);
	char	*	lpb = lpt;
	int			stride = 8 * bm->cwidth - 8;
	char		pat = ~cbytes[color & 3];

	char rx = r, ry = 0;
	int	d = r / 2;
	char * dp;

	y -= clip->top;
	unsigned	h = clip->bottom - clip->top;
	unsigned	y0 = y, y1 = y;

	while (rx > 0)
	{
		int	x0 = x - rx, x1 = x + rx;

		bool	c0 = x0 >= clip->left && x0 < clip->right;
		bool	c1 = x1 >= clip->left && x1 < clip->right;

		if ((unsigned)y0 < h)
		{
			if (c0)
			{
				dp = lpt + (x0 & ~7);
				*dp = ((*dp ^ pat) | ormask[x0 & 7]) ^ pat;
			}
			if (c1)
			{
				dp = lpt + (x1 & ~7);
				*dp = ((*dp ^ pat) | ormask[x1 & 7]) ^ pat;
			}
		}

		if ((unsigned)y1 < h)
		{
			if (c0)
			{
				dp = lpb + (x0 & ~7);
				*dp = ((*dp ^ pat) | ormask[x0 & 7]) ^ pat;
			}
			if (c1)
			{
				dp = lpb + (x1 & ~7);
				*dp = ((*dp ^ pat) | ormask[x1 & 7]) ^ pat;
			}
		}

		if (d >= 0)
		{
			ry++; y0--; y1++;
			d -= ry;
			lpb ++;
			if (!((int)lpb & 7))
				lpb += stride;
			if (!((int)lpt & 7))
				lpt -= stride;
			lpt--;
		}
		if (d < 0)
		{
			rx--;
			d += rx;
		}
	}

	if (x >= clip->left && x < clip->right)
	{
		if ((unsigned)y0 < h)
		{
			dp = lpt + (x & ~7);
			*dp = ((*dp ^ pat) | ormask[x & 7]) ^ pat;
		}
		if ((unsigned)y1 < h)
		{
			dp = lpb + (x & ~7);
			*dp = ((*dp ^ pat) | ormask[x & 7]) ^ pat;
		}
	}
}

void bmmc_circle(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, char color)
{
	if (x - r >= clip->left && x + r < clip->right && y - r >= clip->top && y + r < clip->bottom)
		bmmcu_circle(bm, x, y, r, color);
	else if (x - r < clip->right && x + r >= clip->left && y - r < clip->bottom && y + r >= clip->top)
		bmmc_circle2(bm, clip, x, y, r, color);
}

void bmmc_scan_fill(int left, int right, char * lp, int x0, int x1, char pat)
{
	bm_scan_fill(left, right, lp, x0 & ~1, (x1 + 1) & ~1, pat);
}

void bmmc_circle_fill(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, const char * pattern)
{
	int y0 = y - r, y1 = y + r + 1;
	if (y0 < clip->top)
		y0 = clip->top;
	if (y1 > clip->bottom)
		y1 = clip->bottom;

	const char * pat = pattern;
	char	*	lp = bm->data + bm->cwidth * (y0 & ~7) + (y0 & 7);
	int			stride = 8 * bm->cwidth - 8;

	unsigned rr = r * r + r;
	unsigned d = rr - (y0 - y) * (y0 - y);
	int tt = 2 * (y0 - y) + 1;
	for(char iy=y0; iy<(char)y1; iy++)
	{
		int t = bm_usqrt(d);

		bmmc_scan_fill(clip->left, clip->right, lp, x - t, x + t + 1, pat[iy & 7]);
		lp ++;
		if (!((int)lp & 7))
			lp += stride;

		d -= tt;
		tt += 2;
	}
}

void bmmc_trapezoid_fill(const Bitmap * bm, const ClipRect * clip, long x0, long x1, long dx0, long dx1, int y0, int y1, const char * pattern)
{
	if (y1 <= clip->top || y0 >= clip->bottom)
		return;

	long	tx0 = x0, tx1 = x1;

	if (y1 > clip->bottom)
		y1 = clip->bottom;
	if (y0 < clip->top)
	{
		tx0 += (clip->top - y0) * dx0;
		tx1 += (clip->top - y0) * dx1;
		y0 = clip->top;
	}

	const char * pat = pattern;
	char	*	lp = bm->data + bm->cwidth * (y0 & ~7) + (y0 & 7);
	int			stride = 8 * bm->cwidth - 8;

	for(char iy=y0; iy<(char)y1; iy++)
	{
		bmmc_scan_fill(clip->left, clip->right, lp,  tx0 >> 16, tx1 >> 16, pat[iy & 7]);
		tx0 += dx0;
		tx1 += dx1;
		lp ++;
		if (!((int)lp & 7))
			lp += stride;
	}
}


void bmmc_triangle_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, const char * pat)
{
	int	t;
	if (y1 < y0 && y1 < y2)
	{
		t = y0; y0 = y1; y1 = t;
		t = x0; x0 = x1; x1 = t;
	}
	else if (y2 < y0)
	{
		t = y0; y0 = y2; y2 = t;
		t = x0; x0 = x2; x2 = t;
	}

	if (y2 < y1)
	{
		t = y1; y1 = y2; y2 = t;
		t = x1; x1 = x2; x2 = t;
	}

	if (y0 < y2)
	{
		long	dx1, lx1;
		long	dx2 = ((long)(x2 - x0) << 16) / (y2 - y0);
		long	lx2 = (long)x0 << 16;

		if (y1 > y0)
		{
			dx1 = ((long)(x1 - x0) << 16) / (y1 - y0);

			if (dx1 < dx2)
				bmmc_trapezoid_fill(bm, clip, lx2, lx2, dx1, dx2, y0, y1, pat);
			else
				bmmc_trapezoid_fill(bm, clip, lx2, lx2, dx2, dx1, y0, y1, pat);
			if (y2 == y1)
				return;

			lx2 += dx2 * (y1 - y0);
		}

		dx1 = ((long)(x2 - x1) << 16) / (y2 - y1);
		lx1 = (long)x1 << 16;

		if (lx1 < lx2)
			bmmc_trapezoid_fill(bm, clip, lx1, lx2, dx1, dx2, y1, y2, pat);
		else
			bmmc_trapezoid_fill(bm, clip, lx2, lx1, dx2, dx1, y1, y2, pat);		
	}
}


void bmmc_quad_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, const char * pat)
{
	bmmc_triangle_fill(bm, clip, x0, y0, x1, y1, x2, y2, pat);
	bmmc_triangle_fill(bm, clip, x0, y0, x2, y2, x3, y3, pat);
}


void bmmc_polygon_fill(const Bitmap * bm, const ClipRect * clip, int * px, int * py, char num, const char * pat)
{
	char 	mi = 0;
	int		my = py[0];

	for(char i=1; i<num; i++)
	{
		if (py[i] < my)
		{
			my = py[i];
			mi = i;
		}
	}

	char	li = mi, ri = mi;
	long	lx, rx;

	do
	{
		lx = (long)px[li] << 16;
		if (li == 0)
			li = num;
		li--;
	} while(py[li] == my);

	do
	{
		rx = (long)px[ri] << 16;
		ri++;
		if (ri == num)
			ri = 0;
	}
	while (py[ri] == my);

	int ty = py[li] < py[ri] ? py[li] : py[ri];
	while (ty > my)
	{
		long	dlx = (((long)px[li] << 16) - lx) / (py[li] - my);
		long	drx = (((long)px[ri] << 16) - rx) / (py[ri] - my);

		if (lx < rx || lx == rx && dlx < drx)
			bmmc_trapezoid_fill(bm, clip, lx, rx, dlx, drx, my, ty, pat);
		else
			bmmc_trapezoid_fill(bm, clip, rx, lx, drx, dlx, my, ty, pat);

		lx += (ty - my) * dlx;
		rx += (ty - my) * drx;

		my = ty;

		while (py[li] == my)
		{
			lx = (long)px[li] << 16;
			if (li == 0)
				li = num;
			li--;
		}

		while (py[ri] == my)
		{
			rx = (long)px[ri] << 16;
			ri++;
			if (ri == num)
				ri = 0;
		}

		ty = py[li] < py[ri] ? py[li] : py[ri];
	}

}

struct Edge
{
	char		minY, maxY;
	long		px, dx;
	Edge	*	next;
};

void bmmc_polygon_nc_fill(const Bitmap * bm, const ClipRect * clip, int * px, int * py, char num, const char * pattern)
{
	Edge	*	first = nullptr, * active = nullptr;
	Edge	*	e = (Edge *)BLIT_CODE;

	char	n = num;
	if (n > 16)
		n = 16;

	int	top = clip->top, bottom = clip->bottom;

	for(char i=0; i<n; i++)
	{
		char j = i + 1, k = i;
		if (j >= n)
			j = 0;

		if (py[i] != py[j])
		{
			if (py[i] > py[j])
			{
				k = j; j = i;
			}

			int minY = py[k], maxY = py[j];
			if (minY < bottom && maxY > top)
			{
				e->px = ((long)px[k] << 16) + 0x8000; 
				e->dx = (((long)px[j] << 16) - e->px) / (maxY - minY);

				if (minY < top)
				{
					e->px += e->dx * (top - minY);
					minY = top;
				}
				if (maxY > bottom)
					maxY = bottom;

				e->minY = minY; e->maxY = maxY;

				Edge	*	pp = nullptr, * pe = first;

				while (pe && minY >= pe->minY)
				{
					pp = pe;
					pe = pe->next;
				}
					
				e->next = pe;
				if (pp)
					pp->next = e;
				else
					first = e;

				e++;
			}
		}
	}

	if (first)
	{
		char	y = first->minY;

		const char * pat = pattern;
		char	*	lp = bm->data + bm->cwidth * (y & ~7) + (y & 7);
		int			stride = 8 * bm->cwidth - 8;

		while (first || active)
		{
			while (first && first->minY == y)
			{
				Edge	*	next = first->next;

				Edge	*	pp = nullptr, * pe = active;
				while (pe && (first->px > pe->px || first->px == pe->px && first->dx > pe->dx))
				{
					pp = pe;
					pe = pe->next;
				}

				first->next = pe;
				if (pp)
					pp->next = first;
				else
					active = first;

				first = next;
			}

			Edge	*	e0 = active;
			while (e0)
			{
				Edge	*	e1 = e0->next;
				bmmc_scan_fill(clip->left, clip->right, lp, e0->px >> 16, e1->px >> 16, pat[y & 7]);
				e0 = e1->next;
			}

			lp ++;
			if (!((int)lp & 7))
				lp += stride;

			y++;

			// remove final edges
			Edge	*	pp = nullptr, * pe = active;
			while (pe)
			{
				if (pe->maxY == y)
				{
					if (pp)
						pp->next = pe->next;
					else
						active = pe->next;
				}
				else
				{
					pe->px += pe->dx;
					pp = pe;
				}
				pe = pe->next;
			}
		}
	}
}


#define REG_SP	0x03
#define REG_DP	0x05
#define REG_PAT	0x07
#define REG_S0	0x08
#define REG_S1	0x09
#define REG_D0	0x0a
#define REG_D1	0x0b

static void mbuildline(char ly, char lx, int dx, int dy, int stride, bool left, bool up, char color)
{
	char	ip = 0;

	// ylow
	ip += asm_im(BLIT_CODE + ip, ASM_LDY, ly);
	ip += asm_im(BLIT_CODE + ip, ASM_LDX, lx);

	// set pixel

	ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_SP);
	ip += asm_im(BLIT_CODE + ip, ASM_EOR, color);	
	ip += asm_zp(BLIT_CODE + ip, ASM_ORA, REG_D0);
	ip += asm_im(BLIT_CODE + ip, ASM_EOR, color);
	ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_SP);

	// m >= 0
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
	ip += asm_rl(BLIT_CODE + ip, ASM_BMI, 5 + 15 + 13);

	ip += asm_np(BLIT_CODE + ip, up ? ASM_DEY : ASM_INY);
	ip += asm_im(BLIT_CODE + ip, ASM_CPY, up ? 0xff : 0x08);
	ip += asm_rl(BLIT_CODE + ip, ASM_BNE, 15);

	ip += asm_np(BLIT_CODE + ip, ASM_CLC);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_SP);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, stride & 0xff);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_SP);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_SP + 1);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, stride >> 8);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_SP + 1);
	ip += asm_im(BLIT_CODE + ip, ASM_LDY, up ? 0x07 : 0x00);

	ip += asm_np(BLIT_CODE + ip, ASM_SEC);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP);
	ip += asm_im(BLIT_CODE + ip, ASM_SBC, dx & 0xff);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
	ip += asm_im(BLIT_CODE + ip, ASM_SBC, dx >> 8);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP + 1);

	// m < 0
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
	ip += asm_rl(BLIT_CODE + ip, ASM_BPL, 4 + 15 + 13);

	ip += asm_zp(BLIT_CODE + ip, left ? ASM_ASL : ASM_LSR, REG_D0);
	ip += asm_zp(BLIT_CODE + ip, left ? ASM_ROL : ASM_ROR, REG_D0);
	ip += asm_rl(BLIT_CODE + ip, ASM_BCC, 15);

	ip += asm_zp(BLIT_CODE + ip, left ? ASM_ROL : ASM_ROR, REG_D0);
	ip += asm_np(BLIT_CODE + ip, ASM_CLC);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_SP);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, left ? 0xf8 : 0x08);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_SP);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_SP + 1);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, left ? 0xff : 0x00);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_SP + 1);

	ip += asm_np(BLIT_CODE + ip, ASM_CLC);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, dy & 0xff);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
	ip += asm_im(BLIT_CODE + ip, ASM_ADC, dy >> 8);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP + 1);

	// l --
	ip += asm_np(BLIT_CODE + ip, ASM_DEX);
	ip += asm_rl(BLIT_CODE + ip, ASM_BNE, 2 - ip);
	ip += asm_zp(BLIT_CODE + ip, ASM_DEC, REG_D1);
	ip += asm_rl(BLIT_CODE + ip, ASM_BPL, 2 - ip);

	ip += asm_np(BLIT_CODE + ip, ASM_RTS);
}

#pragma native(mbuildline)

static inline void mcallline(byte * dst, byte bit, int m, char lh)
{
	__asm
	{
		lda	dst
		sta REG_SP
		lda	dst + 1
		sta REG_SP + 1

		lda m
		sta REG_DP
		lda m + 1
		sta REG_DP + 1

		lda lh
		sta REG_D1

		lda bit
		sta REG_D0

		jsr	BLIT_CODE
	}
}

void bmmcu_line(const Bitmap * bm, int x0, int y0, int x1, int y1, char color)
{
	x0 >>= 1;
	x1 >>= 1;

	int dx = x1 - x0, dy = y1 - y0;
	byte	quad = 0;
	if (dx < 0)
	{
		quad = 1;
		dx = -dx;
	}
	if (dy < 0)
	{
		quad |= 2;
		dy = -dy;
	}
	
	int	l;
	if (dx > dy)
		l = dx;
	else
		l = dy;
	
	int	m = dy - dx;
	dx *= 2;
	dy *= 2;

	char	*	dp = bm->data + bm->cwidth * (y0 & ~7) + 2 * (x0 & ~3);
	char		bit = ormask[2 * (x0 & 3)];
	char		ry = y0 & 7;
	int			stride = 8 * bm->cwidth;

	mbuildline(ry, (l + 1) & 0xff, dx, dy, (quad & 2) ? -stride : stride, quad & 1, quad & 2, ~cbytes[color]);

	mcallline(dp, bit, m, l >> 8);
}


static int mmuldiv(int x, int mul, int div)
{
	return (int)((long)x * mul / div);
}

void bmmc_line(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, char color)
{
	int dx = x1 - x0, dy = y1 - y0;

	if (x0 < x1)
	{
		if (x1 < clip->left || x0 >= clip->right)
			return;

		if (x0 < clip->left)
		{
			y0 += mmuldiv(clip->left - x0, dy, dx);
			x0 = clip->left;
		}

		if (x1 >= clip->right)
		{
			y1 -= mmuldiv(x1 + 1 - clip->right, dy, dx);
			x1 = clip->right - 1;
		}
	}
	else if (x1 < x0)
	{
		if (x0 < clip->left || x1 >= clip->right)
			return;

		if (x1 < clip->left)
		{
			y1 += mmuldiv(clip->left - x1, dy, dx);
			x1 = clip->left;
		}

		if (x0 >= clip->right)
		{
			y0 -= mmuldiv(x0 + 1- clip->right, dy, dx);
			x0 = clip->right - 1;
		}		
	}
	else
	{
		if (x0 < clip->left || x0 >= clip->right)
			return;
	}

	if (y0 < y1)
	{
		if (y1 < clip->top || y0 >= clip->bottom)
			return;

		if (y0 < clip->top)
		{
			x0 += mmuldiv(clip->top - y0, dx, dy);
			y0 = clip->top;
		}

		if (y1 >= clip->bottom)
		{
			x1 -= mmuldiv(y1 + 1 - clip->bottom, dx, dy);
			y1 = clip->bottom - 1;
		}
	}
	else if (y1 < y0)
	{
		if (y0 < clip->top || y1 >= clip->bottom)
			return;

		if (y1 < clip->top)
		{
			x1 += mmuldiv(clip->top - y1, dx, dy);
			y1 = clip->top;
		}

		if (y0 >= clip->bottom)
		{
			x0 -= mmuldiv(y0 + 1 - clip->bottom, dx, dy);
			y0 = clip->bottom - 1;
		}		
	}
	else
	{
		if (y0 < clip->top || y0 >= clip->bottom)
			return;
	}

	bmmcu_line(bm, x0, y0, x1, y1, color);
}


void bmmcu_rect_fill(const Bitmap * dbm, int dx, int dy, int w, int h, char color)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;

	bmu_bitblit(dbm, dx, dy, dbm, dx, dy, rx - dx, h, MixedColors[color][color], BLTOP_PATTERN);	
}

void bmmcu_rect_pattern(const Bitmap * dbm, int dx, int dy, int w, int h, const char * pattern)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;

	bmu_bitblit(dbm, dx, dy, dbm, dx, dy, rx - dx, h, pattern, BLTOP_PATTERN);	
}

void bmmcu_rect_copy(const Bitmap * dbm, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;
	sx &= ~1;

	bmu_bitblit(dbm, dx, dy, sbm, sx, sy, rx - dx, h, nullptr, BLTOP_COPY);	
}


void bmmc_rect_fill(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, char color)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;

	bm_bitblit(dbm, clip, dx, dy, dbm, dx, dy, rx - dx, h, MixedColors[color][color], BLTOP_PATTERN);	
}

void bmmc_rect_pattern(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, const char * pattern)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;

	bm_bitblit(dbm, clip, dx, dy, dbm, dx, dy, rx - dx, h, pattern, BLTOP_PATTERN);	
}

void bmmc_rect_copy(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h)
{
	int	rx = (dx + w + 1) & ~1;
	dx &= ~1;
	sx &= ~1;

	bm_bitblit(dbm, clip, dx, dy, sbm, sx, sy, rx - dx, h, nullptr, BLTOP_COPY);	
}

extern byte BLIT_CODE[16 * 14];

inline void bmmc_putdp(char * dp, char x, char c)
{
	dp += 2 * (x & ~3);

	*dp = (*dp & andmask[2 * (x & 3)]) | (c & ormask[2 * (x & 3)]);
}

inline char bmmc_getdp(char * dp, char x)
{
	dp += 2 * (x & ~3);

	return (*dp >> 2 * (3 - (x & 3))) & 3;
}

inline char bmmc_checkdp(char * dp, char x, char c)
{
	dp += 2 * (x & ~3);

	return ((*dp ^ c) & ormask[2 * (x & 3)]);
}


void bmmc_flood_fill(const Bitmap * bm, const ClipRect * clip, int x, int y, char color)
{
	char		bx = (char)(x >> 1), by = (char)y;
	char		sp = 0;
	char		left = clip->left >> 1, right = clip->right >> 1;

	char	*	dp = bm->data + bm->cwidth * (by & ~7) + (by & 7);

	char		back = cbytes[bmmc_getdp(dp, bx)];	

	color = cbytes[color];

	if (back == color)
		return;

	BLIT_CODE[sp++] = bx;
	BLIT_CODE[sp++] = by;

	while (sp > 0)
	{
		by = BLIT_CODE[--sp];
		bx = BLIT_CODE[--sp];

		dp = bm->data + bm->cwidth * (by & ~7) + (by & 7);

		if (bmmc_checkdp(dp, bx, back) == 0)
		{
			bmmc_putdp(dp, bx, color);

			char x0 = bx;
			while (x0 > left && bmmc_checkdp(dp, x0 - 1, back) == 0)
			{
				x0--;
				bmmc_putdp(dp, x0, color);
			}

			char x1 = bx;
			while (x1 + 1 < right && bmmc_checkdp(dp, x1 + 1, back) == 0)
			{
				x1++;
				bmmc_putdp(dp, x1, color);
			}

			bool	check = false;
			if (by > clip->top)
			{
				dp = bm->data + bm->cwidth * ((by - 1) & ~7) + ((by - 1) & 7);

				for(bx=x0; bx<=x1; bx++)
				{
					if (bmmc_checkdp(dp, bx, back) == 0)
					{
						if (!check)
						{
							BLIT_CODE[sp++] = bx;
							BLIT_CODE[sp++] = by - 1;
							check = true;
						}
					}
					else
						check = false;
				}
			}

			check = false;
			if (by + 1 < clip->bottom)
			{
				dp = bm->data + bm->cwidth * ((by + 1) & ~7) + ((by + 1) & 7);

				for(bx=x0; bx<=x1; bx++)
				{
					if (bmmc_checkdp(dp, bx, back) == 0)
					{
						if (!check)
						{
							BLIT_CODE[sp++] = bx;
							BLIT_CODE[sp++] = by + 1;
							check = true;
						}
					}
					else
						check = false;
				}
			}
		}
	}
}

#pragma native(bmmc_flood_fill)
