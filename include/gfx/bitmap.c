#include "bitmap.h"
#include "tinyfont.h"
#include <stdlib.h>
#include <c64/asm6502.h>
#include <string.h>
#include <fixmath.h>

void bm_init(Bitmap * bm, char * data, char cw, char ch)
{
	bm->rdata = nullptr;
	bm->data = data;
	bm->cwidth = cw;
	bm->cheight = ch;
	bm->width = cw * 8;
}

void bm_alloc(Bitmap * bm, char cw, char ch)
{
	bm->rdata = malloc(cw * ch * 8 + 8);
	bm->data = bm->rdata + 8 - ((int)bm->rdata & 7);
	bm->cwidth = cw;
	bm->cheight = ch;
	bm->width = cw * 8;
}

void bm_free(Bitmap * bm)
{
	free(bm->rdata);
}

void bm_fill(const Bitmap * bm, char data)
{
	memset(bm->data, data, bm->cwidth * bm->cheight * 8);
}


void bm_set(const Bitmap * bm, int x, int y)
{
	bm->data[bm->cwidth * (y & ~7) + (x & ~7) + (y & 7)] |= 0x80 >> (x & 7);
}

#pragma native(bm_set)

void bm_clr(const Bitmap * bm, int x, int y)
{
	bm->data[bm->cwidth * (y & ~7) + (x & ~7) + (y & 7)] &= ~(0x80 >> (x & 7));
}

#pragma native(bm_clr)

bool bm_get(const Bitmap * bm, int x, int y)
{
	return (bm->data[bm->cwidth * (y & ~7) + (x & ~7) + (y & 7)] & (0x80 >> (x & 7))) != 0;
}

#pragma native(bm_get)

void bm_put(const Bitmap * bm, int x, int y, bool c)
{
	char	*	dp = bm->data + bm->cwidth * (y & ~7) + (x & ~7) + (y & 7);
	char		m = 0x80 >> (x & 7);
	if (c)
		*dp |= m;
	else
		*dp &= ~m;
}

#pragma native(bm_put)

char NineShadesOfGrey[9][8] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 0
	{0x22, 0x00, 0x88, 0x00, 0x22, 0x00, 0x88, 0x00}, // 8
	{0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88}, // 16
	{0x88, 0x55, 0x22, 0x55, 0x88, 0x55, 0x22, 0x55}, // 24
	{0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55}, // 32
	{0xbb, 0x55, 0xee, 0x55, 0xbb, 0x55, 0xee, 0x55}, // 40
	{0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77}, // 48
	{0xff, 0xee, 0xff, 0xbb, 0xff, 0xee, 0xff, 0xbb}, // 56
	{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // 64
};

byte BLIT_CODE[16 * 14];


static const byte lmask[8] = {0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01};
static const byte rmask[8] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe};

void bm_scan_fill(int left, int right, char * lp, int x0, int x1, char pat)
{
	if (x0 < left)
		x0 = left;
	if (x1 > right)
		x1 = right;

	if (x1 > x0)
	{
		char	*	dp = lp + (x0 & ~7);
		char		l = (x1 >> 3) - (x0 >> 3);
		char 		lm = lmask[x0 & 7], rm = rmask[x1 & 7];
		char 		o = 0;

		if (l == 0)
			rm &= lm;
		else
		{
			*dp = (*dp & ~lm) | (pat & lm);
			o = 8;
			if (l >= 32)
			{					
				for(char i=0; i<31; i++)
				{
					dp[o] = pat;
					o += 8;
				}
				dp += 256;
				l -= 31;
			}
			for(char i=1; i<l; i++)
			{
				dp[o] = pat;
				o += 8;
			}
		}
		dp[o] = (dp[o] & ~rm) | (pat & rm);
	}		
}

#pragma native(bm_scan_fill)

unsigned bm_usqrt(unsigned n)
{
	unsigned p, q, r, h;

	p = 0;
    r = n;

   	q = 0x4000;
   	while (q > r)
   		q >>= 2;

    while (q != 0)
    {
        h = p + q;
        p >>= 1;
        if (r >= h)
        {
            p += q;
            r -= h;
        } 
        q >>= 2;
    }

    return p;
}

#pragma native(bm_usqrt)

void bm_circle_fill(const Bitmap * bm, const ClipRect * clip, int x, int y, char r, const char * pattern)
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
	for(char iy=y0; iy<(char)y1; iy++)
	{
		int d = (iy - y);

		int t = bm_usqrt(rr - d * d);

		bm_scan_fill(clip->left, clip->right, lp, x - t, x + t + 1, pat[iy & 7]);
		lp ++;
		if (!((int)lp & 7))
			lp += stride;
	}
}

void bm_trapezoid_fill(const Bitmap * bm, const ClipRect * clip, long x0, long x1, long dx0, long dx1, int y0, int y1, const char * pattern)
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
		bm_scan_fill(clip->left, clip->right, lp,  tx0 >> 16, tx1 >> 16, pat[iy & 7]);
		tx0 += dx0;
		tx1 += dx1;
		lp ++;
		if (!((int)lp & 7))
			lp += stride;
	}
}


void bm_triangle_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, const char * pat)
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
				bm_trapezoid_fill(bm, clip, lx2, lx2, dx1, dx2, y0, y1, pat);
			else
				bm_trapezoid_fill(bm, clip, lx2, lx2, dx2, dx1, y0, y1, pat);
			if (y2 == y1)
				return;

			lx2 += dx2 * (y1 - y0);
		}

		dx1 = ((long)(x2 - x1) << 16) / (y2 - y1);
		lx1 = (long)x1 << 16;

		if (lx1 < lx2)
			bm_trapezoid_fill(bm, clip, lx1, lx2, dx1, dx2, y1, y2, pat);
		else
			bm_trapezoid_fill(bm, clip, lx2, lx1, dx2, dx1, y1, y2, pat);		
	}
}


void bm_quad_fill(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, const char * pat)
{
	bm_triangle_fill(bm, clip, x0, y0, x1, y1, x2, y2, pat);
	bm_triangle_fill(bm, clip, x0, y0, x2, y2, x3, y3, pat);
}

void bm_polygon_fill(const Bitmap * bm, const ClipRect * clip, int * px, int * py, char num, const char * pat)
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
			bm_trapezoid_fill(bm, clip, lx, rx, dlx, drx, my, ty, pat);
		else
			bm_trapezoid_fill(bm, clip, rx, lx, drx, dlx, my, ty, pat);

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

void bm_polygon_nc_fill(const Bitmap * bm, const ClipRect * clip, int * px, int * py, char num, const char * pattern)
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
				bm_scan_fill(clip->left, clip->right, lp, e0->px >> 16, e1->px >> 16, pat[y & 7]);
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

static inline void buildline(char ly, char lx, int dx, int dy, int stride, bool left, bool up, char pattern, LineOp op)
{
	char	ip = 0;
	bool	delta16 = ((dx | dy) & 0xff80) != 0;

	// ylow
	ip += asm_im(BLIT_CODE + ip, ASM_LDY, ly);
	ip += asm_im(BLIT_CODE + ip, ASM_LDX, lx);

	// set pixel
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_D0);

	switch (op)
	{
	case LINOP_SET:
		ip += asm_zp(BLIT_CODE + ip, ASM_ASL, REG_PAT);
		ip += asm_rl(BLIT_CODE + ip, ASM_BCC, 6);
		ip += asm_zp(BLIT_CODE + ip, ASM_INC, REG_PAT);
		ip += asm_iy(BLIT_CODE + ip, ASM_ORA, REG_SP);
		ip += asm_rl(BLIT_CODE + ip, ASM_BNE, 4);
		ip += asm_im(BLIT_CODE + ip, ASM_EOR, 0xff);	
		ip += asm_iy(BLIT_CODE + ip, ASM_AND, REG_SP);
		ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_SP);
		break;
	case LINOP_OR:
		if (pattern != 0xff)
		{
			ip += asm_zp(BLIT_CODE + ip, ASM_ASL, REG_PAT);
			ip += asm_rl(BLIT_CODE + ip, ASM_BCC, 6);
			ip += asm_zp(BLIT_CODE + ip, ASM_INC, REG_PAT);
		}
		ip += asm_iy(BLIT_CODE + ip, ASM_ORA, REG_SP);
		ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_SP);
		break;
	case LINOP_XOR:
		if (pattern != 0xff)
		{
			ip += asm_zp(BLIT_CODE + ip, ASM_ASL, REG_PAT);
			ip += asm_rl(BLIT_CODE + ip, ASM_BCC, 6);
			ip += asm_zp(BLIT_CODE + ip, ASM_INC, REG_PAT);
		}
		ip += asm_iy(BLIT_CODE + ip, ASM_EOR, REG_SP);
		ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_SP);
		break;
	case LINOP_AND:
		if (pattern != 0xff)
		{
			ip += asm_zp(BLIT_CODE + ip, ASM_ASL, REG_PAT);
			ip += asm_rl(BLIT_CODE + ip, ASM_BCC, 8);
			ip += asm_zp(BLIT_CODE + ip, ASM_INC, REG_PAT);
		}
		ip += asm_im(BLIT_CODE + ip, ASM_EOR, 0xff);	
		ip += asm_iy(BLIT_CODE + ip, ASM_AND, REG_SP);
		ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_SP);
		break;
	}

	// m >= 0
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + delta16);
	ip += asm_rl(BLIT_CODE + ip, ASM_BMI, delta16 ? 5 + 15 + 13 + 2 : 5 + 15 + 7 + 2);

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

	if (delta16)
	{
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
		ip += asm_im(BLIT_CODE + ip, ASM_SBC, dx >> 8);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP + 1);
	}

	// m < 0
	ip += asm_rl(BLIT_CODE + ip, ASM_BPL, delta16 ? 4 + 15 + 13 : 4 + 15 + 7);

	ip += asm_zp(BLIT_CODE + ip, left ? ASM_ASL : ASM_LSR, REG_D0);
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
	if (delta16)
	{
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_DP + 1);
		ip += asm_im(BLIT_CODE + ip, ASM_ADC, dy >> 8);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_DP + 1);
	}

	// l --
	ip += asm_np(BLIT_CODE + ip, ASM_DEX);
	ip += asm_rl(BLIT_CODE + ip, ASM_BNE, 2 - ip);
	ip += asm_zp(BLIT_CODE + ip, ASM_DEC, REG_D1);
	ip += asm_rl(BLIT_CODE + ip, ASM_BPL, 2 - ip);

	ip += asm_np(BLIT_CODE + ip, ASM_RTS);
}

#pragma native(buildline)

static inline void callline(byte * dst, byte bit, int m, char lh, char pattern)
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

		lda pattern
		sta REG_PAT

		jsr	BLIT_CODE
	}
}

void bmu_line(const Bitmap * bm, int x0, int y0, int x1, int y1, char pattern, LineOp op)
{
	if (pattern == 0x00)
	{
		if (op == LINOP_SET)
		{
			pattern = 0xff;
			op = LINOP_AND;
		}
		else
			return;
	}
	else if (pattern == 0xff && op == LINOP_SET)
		op = LINOP_OR;

	int dx = x1 - x0, dy = y1 - y0;
	bool	left = false;
	bool	up = false;

	if (dx < 0)
	{
		left = true;
		dx = -dx;
	}
	if (dy < 0)
	{
		up = true;
		dy = -dy;
	}
	
	int	l;
	if ((unsigned)dx > (unsigned)dy)
		l = dx;
	else
		l = dy;
	
	int	m = dy - dx;
	if (m & 1)
	{
		dx *= 2;
		dy *= 2;	
	}
	else
	{
		m >>= 1;
	}

	char	*	dp = bm->data + bm->cwidth * (y0 & ~7) + (x0 & ~7);
	char		bit = 0x80 >> (x0 & 7);
	char		ry = y0 & 7;
	int			stride = 8 * bm->cwidth;

	buildline(ry, (l + 1) & 0xff, dx, dy, up ? -stride : stride, left, up, pattern, op);

	callline(dp, bit, m, l >> 8, pattern);
}

void bm_line(const Bitmap * bm, const ClipRect * clip, int x0, int y0, int x1, int y1, char pattern, LineOp op)
{
	int dx = x1 - x0, dy = y1 - y0;

	if (dx > 0)
	{
		if (x1 < clip->left || x0 >= clip->right)
			return;

		if (x0 < clip->left)
		{
			y0 += lmuldiv16s(clip->left - x0, dy, dx);
			x0 = clip->left;
		}

		if (x1 >= clip->right)
		{
			y1 -= lmuldiv16s(x1 + 1 - clip->right, dy, dx);
			x1 = clip->right - 1;
		}
	}
	else
	{
		if (x0 < clip->left || x1 >= clip->right)
			return;

		if (x1 < clip->left)
		{
			y1 += lmuldiv16s(clip->left - x1, dy, dx);
			x1 = clip->left;
		}

		if (x0 >= clip->right)
		{
			y0 -= lmuldiv16s(x0 + 1- clip->right, dy, dx);
			x0 = clip->right - 1;
		}		
	}

	if (dy > 0)
	{
		if (y1 < clip->top || y0 >= clip->bottom)
			return;

		if (y0 < clip->top)
		{
			x0 += lmuldiv16s(clip->top - y0, dx, dy);
			y0 = clip->top;
		}

		if (y1 >= clip->bottom)
		{
			x1 -= lmuldiv16s(y1 + 1 - clip->bottom, dx, dy);
			y1 = clip->bottom - 1;
		}
	}
	else
	{
		if (y0 < clip->top || y1 >= clip->bottom)
			return;

		if (y1 < clip->top)
		{
			x1 += lmuldiv16s(clip->top - y1, dx, dy);
			y1 = clip->top;
		}

		if (y0 >= clip->bottom)
		{
			x0 -= lmuldiv16s(y0 + 1 - clip->bottom, dx, dy);
			y0 = clip->bottom - 1;
		}		
	}

	bmu_line(bm, x0, y0, x1, y1, pattern, op);
}

static inline void callddop(byte * src, byte * dst, byte pat)
{
	__asm
	{
		lda	src
		sta REG_SP
		lda	src + 1
		sta REG_SP + 1
		lda	dst
		sta REG_DP
		lda dst + 1
		sta REG_DP + 1
		lda pat
		sta REG_PAT
		jsr	BLIT_CODE
	}
}

// build code to fetch src, result in accu

static char builddop_src(char ip, char shift, bool reverse)
{
	if (shift != 0)
	{
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_S0);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_S1);

		ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_SP);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_S0);

		if (reverse)
		{
			if (shift > 4)
			{
				for(char i=shift; i<8; i++)
				{
					ip += asm_zp(BLIT_CODE + ip, ASM_ASL, REG_S1);
					ip += asm_ac(BLIT_CODE + ip, ASM_ROL);
				}
			}
			else
			{
				for(char i=0; i<shift; i++)
				{
					ip += asm_ac(BLIT_CODE + ip, ASM_LSR);
					ip += asm_zp(BLIT_CODE + ip, ASM_ROR, REG_S1);
				}
				ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_S1);
			}
		}
		else
		{
			if (shift > 4)
			{
				for(char i=shift; i<8; i++)
				{
					ip += asm_ac(BLIT_CODE + ip, ASM_ASL);
					ip += asm_zp(BLIT_CODE + ip, ASM_ROL, REG_S1);
				}
				ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_S1);
			}
			else
			{
				for(char i=0; i<shift; i++)
				{
					ip += asm_zp(BLIT_CODE + ip, ASM_LSR, REG_S1);
					ip += asm_ac(BLIT_CODE + ip, ASM_ROR);
				}
			}
		}
	}
	else
		ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_SP);

	return ip;
}

#pragma native(builddop_src)

static AsmIns blitops_op[4] = {ASM_BRK, ASM_AND, ASM_ORA, ASM_EOR};

static char builddop_op(char ip, BlitOp op)
{
	char	reg = REG_D0;
	if (op & BLIT_PATTERN)
		reg = REG_PAT;

	if (op & BLIT_IMM)
	{
		if (op & BLIT_INVERT)
			ip += asm_im(BLIT_CODE + ip, ASM_LDA, 0xff);
		else
			ip += asm_im(BLIT_CODE + ip, ASM_LDA, 0x00);
	}
	else if (!(op & BLIT_SRC))
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, reg);
	else if (op & BLIT_INVERT)
		ip += asm_im(BLIT_CODE + ip, ASM_EOR, 0xff);

	op &= BLIT_OP;
	if (op)
		ip += asm_zp(BLIT_CODE + ip, blitops_op[op], reg);

	return ip;
}

#pragma native(builddop_op)

static void builddop(char shift, char w, char lmask, char rmask, BlitOp op, bool reverse)
{
	byte	ip = 0;

	bool	usesrc = op & BLIT_SRC;
	bool	usedst = op & BLIT_DST;

	char	asm_clc = ASM_CLC, asm_adc = ASM_ADC, asm_bcc = ASM_BCC, asm_inc = ASM_INC, ystart = 0;
	if (reverse)
	{
		asm_clc = ASM_SEC;
		asm_adc = ASM_SBC;
		asm_bcc = ASM_BCS;
		asm_inc = ASM_DEC;
		ystart = 0xf8;
	}

	ip += asm_im(BLIT_CODE + ip, ASM_LDY, ystart);

	ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_DP);
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_D0);

	if (usesrc)
	{
		ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_SP);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_S0);

		ip += asm_np(BLIT_CODE + ip, asm_clc);
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_SP);
		ip += asm_im(BLIT_CODE + ip, asm_adc, 8);	
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_SP);
		ip += asm_rl(BLIT_CODE + ip, asm_bcc, 2);
		ip += asm_zp(BLIT_CODE + ip, asm_inc, REG_SP + 1);
	
		ip = builddop_src(ip, shift, reverse);
	}

	if (w == 0)
		lmask &= rmask;

	ip = builddop_op(ip, op);
	ip += asm_im(BLIT_CODE + ip, ASM_AND, lmask);	
	ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_S1);
	ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_D0);
	ip += asm_im(BLIT_CODE + ip, ASM_AND, ~lmask);	
	ip += asm_zp(BLIT_CODE + ip, ASM_ORA, REG_S1);
	ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_DP);

	if (w > 0)
	{
		ip += asm_np(BLIT_CODE + ip, asm_clc);
		ip += asm_np(BLIT_CODE + ip, ASM_TYA);
		ip += asm_im(BLIT_CODE + ip, asm_adc, 8);	
		ip += asm_np(BLIT_CODE + ip, ASM_TAY);

		if (w > 1)
		{
			ip += asm_im(BLIT_CODE + ip, ASM_LDX, w - 1);	
			char lp = ip;

			if (usedst)
			{
				ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_DP);
				ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_D0);
			}

			if (usesrc)
			{
				ip = builddop_src(ip, shift, reverse);
			}

			ip = builddop_op(ip, op);
			ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_DP);

			ip += asm_np(BLIT_CODE + ip, asm_clc);
			ip += asm_np(BLIT_CODE + ip, ASM_TYA);
			ip += asm_im(BLIT_CODE + ip, asm_adc, 8);	
			ip += asm_np(BLIT_CODE + ip, ASM_TAY);
			if (w > 31)
			{
				if (usesrc)
				{
					ip += asm_rl(BLIT_CODE + ip, asm_bcc, 4);
					ip += asm_zp(BLIT_CODE + ip, asm_inc, REG_SP + 1);
				}
				else
				{
					ip += asm_rl(BLIT_CODE + ip, asm_bcc, 2);		
				}
				ip += asm_zp(BLIT_CODE + ip, asm_inc, REG_DP + 1);
			}

			ip += asm_np(BLIT_CODE + ip, ASM_DEX);
			ip += asm_rl(BLIT_CODE + ip, ASM_BNE, lp - ip - 2);
		}

		ip += asm_iy(BLIT_CODE + ip, ASM_LDA, REG_DP);
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_D0);

		if (usesrc)
		{
			ip = builddop_src(ip, shift, reverse);
		}

		ip = builddop_op(ip, op);
		ip += asm_im(BLIT_CODE + ip, ASM_AND, rmask);	
		ip += asm_zp(BLIT_CODE + ip, ASM_STA, REG_S1);
		ip += asm_zp(BLIT_CODE + ip, ASM_LDA, REG_D0);
		ip += asm_im(BLIT_CODE + ip, ASM_AND, ~rmask);	
		ip += asm_zp(BLIT_CODE + ip, ASM_ORA, REG_S1);
		ip += asm_iy(BLIT_CODE + ip, ASM_STA, REG_DP);
	}

	ip += asm_np(BLIT_CODE + ip, ASM_RTS);
}

#pragma native(builddop)

void bmu_bitblit(const Bitmap * dbm, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op)
{
	int			rx = dx + w;
	char		dxh0 = dx >> 3, dxh1 = rx >> 3;

	char		lm = lmask[dx & 7], rm = rmask[rx & 7];

	char		cw = dxh1 - dxh0;

	bool	reverse = dbm == sbm && (dy > sy || dy == sy && dx > sx);

	if (reverse)
	{
		dy += h;
		sy += h;
	}

	char	*	dp = dbm->data + dbm->cwidth * (dy & ~7) + (dx & ~7) + (dy & 7);
	char	*	sp = sbm->data + sbm->cwidth * (sy & ~7) + (sx & ~7) + (sy & 7);

	if (reverse)
	{
		sp += 8 * cw + 8 - 0xf8;
		dp += 8 * cw - 0xf8;
		byte	t = lm; lm = rm; rm = t;
	}

	char		shift;
	if ((dx & 7) > (sx & 7))
	{
		shift = (dx & 7) - (sx & 7);
		sp -= 8;
	}
	else
	{		
		shift = 8 + (dx & 7) - (sx & 7);
	}

	builddop(shift, cw, lm, rm, op, reverse);

	const char * pat = pattern;

	int	sstride = 8 * sbm->cwidth - 8;
	int	dstride = 8 * dbm->cwidth - 8;

	if (reverse)
	{
		sstride = -sstride;
		dstride = -dstride;

		for(char y=h; y>0; y--)
		{		
			if (((int)sp & 7) == 0)
				sp += sstride;
			sp--;

			if (((int)dp & 7) == 0)
				dp += dstride;
			dp--;

			char pi = (int)dp & 7;

			callddop(sp, dp, pat[pi]);
		}
	}
	else
	{
		for(char y=h; y>0; y--)
		{	
			char pi = (int)dp & 7;

			callddop(sp, dp, pat[pi]);

			sp++;
			if (((int)sp & 7) == 0)
				sp += sstride;

			dp++;
			if (((int)dp & 7) == 0)
				dp += dstride;
		}
	}
}

#pragma native(bmu_bitblit)

void bm_bitblit(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h, const char * pattern, BlitOp op)
{
	if (dx >= clip->right || dy >= clip->bottom)
		return;

	if (dx < clip->left)
	{
		int	d = clip->left - dx;
		dx += d;
		sx += d;
		w -= d;
	}

	if (dy < clip->top)
	{
		int	d = clip->top - dy;
		dy += d;
		sy += d;
		h -= d;
	}

	if (dx + w > clip->right)
		w = clip->right - dx;

	if (dy + h > clip->bottom)
		h = clip->bottom - dy;

	if (w > 0 && h > 0)
		bmu_bitblit(dbm, dx, dy, sbm, sx, sy, w, h, pattern, op);
}

inline void bmu_rect_fill(const Bitmap * dbm, int dx, int dy, int w, int h)
{
	bmu_bitblit(dbm, dx, dy, dbm, dx, dy, w, h, nullptr, BLTOP_SET);	
}

inline void bmu_rect_clear(const Bitmap * dbm, int dx, int dy, int w, int h)
{
	bmu_bitblit(dbm, dx, dy, dbm, dx, dy, w, h, nullptr, BLTOP_RESET);	
}

inline void bmu_rect_pattern(const Bitmap * dbm, int dx, int dy, int w, int h, const char * pattern)
{
	bmu_bitblit(dbm, dx, dy, dbm, dx, dy, w, h, pattern, BLTOP_PATTERN);	
}

inline void bmu_rect_copy(const Bitmap * dbm, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h)
{
	bmu_bitblit(dbm, dx, dy, sbm, sx, sy, w, h, nullptr, BLTOP_COPY);	
}


inline void bm_rect_fill(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h)
{
	bm_bitblit(dbm, clip, dx, dy, dbm, dx, dy, w, h, nullptr, BLTOP_SET);	
}

inline void bm_rect_clear(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h)
{
	bm_bitblit(dbm, clip, dx, dy, dbm, dx, dy, w, h, nullptr, BLTOP_RESET);	
}

inline void bm_rect_pattern(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, const char * pattern)
{
	bm_bitblit(dbm, clip, dx, dy, dbm, dx, dy, w, h, pattern, BLTOP_PATTERN);	
}

inline void bm_rect_copy(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, const Bitmap * sbm, int sx, int sy, int w, int h)
{
	bm_bitblit(dbm, clip, dx, dy, sbm, sx, sy, w, h, nullptr, BLTOP_COPY);	
}

static char	tworks[8];

int bmu_text(const Bitmap * bm, const char * str, char len)
{
	char lx = 0;
	int  tw = 0;

	char * cp = bm->data;

	for(char fx=0; fx<len; fx++)
	{
		char ch = str[fx];

		ch -= 32;

		char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];

		const char	*	fp = TinyFont + 192 + (f0 + ((f1 & 3) << 8));

		char w = f1 >> 2;
		tw += w + 1;

		for(char px=0; px<w; px++)
		{
			char	b = fp[px];
			__asm
			{
				lda		b
				asl
				rol		tworks + 0
				asl
				rol		tworks + 1
				asl
				rol		tworks + 2
				asl
				rol		tworks + 3
				asl
				rol		tworks + 4
				asl
				rol		tworks + 5
				asl
				rol		tworks + 6
				asl
				rol		tworks + 7
			}
			lx++;
			if (lx == 8)
			{
				cp[0] = tworks[0];
				cp[1] = tworks[1];
				cp[2] = tworks[2];
				cp[3] = tworks[3];
				cp[4] = tworks[4];
				cp[5] = tworks[5];
				cp[6] = tworks[6];
				cp[7] = tworks[7];

				cp += 8;
				lx = 0;
			}
		}

		__asm
		{
			asl		tworks + 0
			asl		tworks + 1
			asl		tworks + 2
			asl		tworks + 3
			asl		tworks + 4
			asl		tworks + 5
			asl		tworks + 6
			asl		tworks + 7
		}

		lx++;
		if (lx == 8)
		{
			cp[0] = tworks[0];
			cp[1] = tworks[1];
			cp[2] = tworks[2];
			cp[3] = tworks[3];
			cp[4] = tworks[4];
			cp[5] = tworks[5];
			cp[6] = tworks[6];
			cp[7] = tworks[7];

			cp += 8;
			lx = 0;
		}

	}

	while (lx < 8)
	{
		__asm
		{
			asl		tworks + 0
			asl		tworks + 1
			asl		tworks + 2
			asl		tworks + 3
			asl		tworks + 4
			asl		tworks + 5
			asl		tworks + 6
			asl		tworks + 7
		}
		lx++;
	}

	cp[0] = tworks[0];
	cp[1] = tworks[1];
	cp[2] = tworks[2];
	cp[3] = tworks[3];
	cp[4] = tworks[4];
	cp[5] = tworks[5];
	cp[6] = tworks[6];
	cp[7] = tworks[7];

	return tw;
}

#pragma native(bmu_text)

int bm_text_size(const char * str, char len)
{
	int tw = 0;

	for(char fx=0; fx<len; fx++)
	{
		char ch = str[fx] - 32;
		char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];
		tw += (f1 >> 2) + 1;
	}

	return tw;
}

static char tbuffer[320];
#pragma align(tbuffer, 8)

static Bitmap tbitmap = {
	tbuffer, nullptr, 40, 1, 320
};

int bmu_put_chars(const Bitmap * bm, int x, int y, const char * str, char len, BlitOp op)
{
	int	tw = bmu_text(&tbitmap, str, len);

	bmu_bitblit(bm, x, y, &tbitmap, 0, 0, tw, 8, nullptr, op);

	return tw;
}

int bm_put_chars(const Bitmap * bm, const ClipRect * clip, int x, int y, const char * str, char len, BlitOp op)
{
	int tw = 0;

	if (y >= clip->bottom || x >= clip->right || y + 8 <= clip->top)
	{
		for(char fx=0; fx<len; fx++)
		{
			char ch = str[fx] - 32;
			char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];
			tw += (f1 >> 2) + 1;
		}

		return tw;
	}

	if (x < clip->left)
	{
		char fx = 0;		
		while (fx < len)
		{
			char ch = str[fx] - 32;
			char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];
			char w = (f1 >> 2) + 1;
			if (x + w >= clip->left)
				break;
			tw += w;
			x += w;
			fx++;
		}

		str += fx;
		len -= fx;
	}

	int rx = x;
	char fx = 0;
	while (fx < len && rx < clip->right)
	{
		char ch = str[fx] - 32;
		char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];
		char w = (f1 >> 2) + 1;
		rx += w;
		tw += w;
		fx++;
	}

	int cw = bmu_text(&tbitmap, str, fx);

	bm_bitblit(bm, clip, x, y, &tbitmap, 0, 0, cw, 8, nullptr, op);

	while (fx < len)
	{
		char ch = str[fx] - 32;
		char f0 = TinyFont[ch], f1 = TinyFont[ch + 96];
		char w = (f1 >> 2) + 1;
		tw += w;
		fx++;
	}

	return tw;
}

int bm_put_string(const Bitmap * bm, const ClipRect * clip, int x, int y, const char * str, BlitOp op)
{
	return bm_put_chars(bm, clip, x, y, str, strlen(str), op);
}

int bm_transform(const Bitmap * dbm, const ClipRect * clip, int dx, int dy, int w, int h, const Bitmap * sbm, int sx, int sy, int dxx, int dxy, int dyx, int dyy)
{
	long	lsx = (long)sx << 16, lsy = (long)sy << 16;

	char 	*	ldp = dbm->data + dbm->cwidth * (dy & ~7) + (dx & ~7) + (dy & 7);
	int			dstride = dbm->cwidth * 8 - 8;

	for(int y=0; y<h; y++)
	{
		long	rsx = lsx, rsy = lsy;

		char	*	dp = ldp;
		char		m = 0x80 >> (dx & 7);

		for(int x=0; x<w; x++)
		{
			int	ix = (int)(rsx >> 16);
			int	iy = (int)(rsy >> 16);

			if (ix >= 0 && iy >= 0 && ix < sbm->width && iy < 8 * sbm->cheight)
			{
				if (bm_get(sbm, ix, iy))
					*dp |= m;
				else
					*dp &= ~m;
			}
			m >>= 1;
			if (!m)
			{
				m = 0x80;
				dp += 8;
			}

			rsx += (long)dxx << 8;
			rsy += (long)dyx << 8;
		}

		ldp++;
		if (((int)ldp & 7) == 0)
			ldp += dstride;

		lsx += (long)dxy << 8;
		lsy += (long)dyy << 8;
	}
}

#pragma native(bm_transform)
