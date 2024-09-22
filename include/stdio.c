#include "stdio.h"
#include "conio.h"
#include "stdlib.h"
#include "stdbool.h"

#if defined(__C128__)
#pragma code(lowcode)
__asm bsout
{	
		ldx #0
		stx 0xff00
		jsr 0xffd2
		sta 0xff01
}
__asm bsplot
{	
		lda #0
		sta 0xff00
		jsr 0xfff0
		sta 0xff01
}
__asm bsin
{
		lda #0
		sta 0xff00
		jsr 0xffcf
		sta 0xff01	
}

#pragma code(code)
#elif defined(__PLUS4__)
#pragma code(lowcode)
__asm bsout
{	
		sta 0xff3e
		jsr 0xffd2
		sta 0xff3f
}
__asm bsin
{
		sta 0xff3e
		jsr 0xffe4
		sta 0xff3f
}
__asm bsplot
{	
		sta 0xff3e
		jsr 0xfff0
		sta 0xff3f
}
#pragma code(code)
#elif defined(__ATARI__)
__asm bsout
{
		tax
		lda	0xe407
		pha
		lda 0xe406
		pha
		txa
}

__asm bsin
{
		lda	0xe405
		pha
		lda 0xe404
		pha
}

__asm bsplot
{

}

#else
#define bsout	0xffd2
#define bsplot	0xfff0
#define bsin	0xffcf
#endif

__asm putpch
{
#if defined(__ATARI__)
		cmp #10
		bne	w1
		lda #0x9b
	w1:
		jmp	bsout
#else
		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp #10
		bne	w1
		lda #13
	w1:
		cmp #9
		beq t1

		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #65
		bcc w3
		cmp	#123
		bcs	w3
#if defined(__CBMPET__)
		cmp	#97
		bcs	w4
		cmp #91
		bcs	w3
	w2:
		eor	#$a0
	w4:
		eor #$20

#else
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
#endif
		cpx #IOCHM_PETSCII_2
		beq	w3
		and #$df
	w3:
		jmp	bsout
	t1:
		sec
		jsr bsplot
		tya
		and	#3
		eor #3
		tax
		lda #$20
	l1:
		jsr bsout
		dex
		bpl l1
#endif		
}

__asm getpch
{
		jsr	bsin
#if !defined(__ATARI__)
		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp	#13
		bne	w1
		lda #10
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #219
		bcs w3
		cmp #65
		bcc w3

		cmp #193
		bcc w4
		eor #$a0
	w4:
		cmp	#123
		bcs	w3
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
	w3:
#endif	
}

void putchar(char c)
{
	__asm {
		lda c
		jsr	putpch
	}
}

char getchar(void)
{
	__asm {
		jsr	getpch
		sta	accu
		lda	#0
		sta	accu + 1
	}
}

void puts(const char * str)
{
	__asm {
	ploop:
		ldy	#0
		lda	(str), y
		beq	pdone
	
		jsr	putpch

		inc	str
		bne	ploop
		inc	str + 1
		bne	ploop
	pdone:
	}
}

char * gets(char * str)
{
	__asm {
	gloop:
		jsr	getpch
		ldy	#0
		cmp	#10
		beq	gdone
		sta	(str), y
		inc	str
		bne	gloop
		inc	str + 1
		bne	gloop
	gdone:		
		lda	#0
		sta	(str), y
	}
	
	return str;
}

char* gets_s(char* str, size_t n)
{
	if (str == NULL)
		return NULL;

	if (n == 0 || n == 1)
		return NULL;
	str[0] = '\0';
	size_t leftToStore = n - 1;
	bool isTruncated = false;

	__asm {
		ldy #0

	read_loop:
		jsr getpch

		// Check if the read in character is the line feed.
		cmp #10
		beq read_done

		// Check to see if the maximum characters is reached.
		ldx leftToStore
		cpx #0
		beq read_max

		// Store the read in character & prepare for the next read.
		sta (str), Y
		iny
		dec leftToStore
		jmp read_loop

	read_max:
		ldx #1
		stx isTruncated

	read_done:
		// Place a null character at the end of the C-string.
		lda #0
		sta (str), Y
	}

	if (isTruncated)
		return NULL;
	return str;
}

typedef void * (* putstrfn)(void * handle, const char * str);

void * putstrio(void * handle, const char * str)
{
	puts(str);
	return handle;
}

void * putstrstr(void * handle, const char * str)
{
	char * d = (char *)handle;
	const char * s = str;

	do {} while (*d++ = *s++);

	return d - 1;
}

struct sinfo
{
	char		fill;
	char		width, precision;
	unsigned	base;
	bool		sign, left, prefix;
};

int nformi(const sinfo * si, char * str, int v, bool s)
{
	char	buffer[16];

	unsigned int u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	char	i = 16;
	while (u > 0)
	{
		int	c = u % si->base;
		c += c >= 10 ? 'A' - 10 : '0';
		buffer[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 16 - si->precision : 15;

	while (i > digits)
		buffer[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		buffer[--i] = 'X';
		buffer[--i] = '0';
	}

	if (neg)
		buffer[--i] = '-';
	else if (si->sign)
		buffer[--i] = '+';

	char j = 0;
	if (si->left)
	{
		while (i < 16)
			str[j++] = buffer[i++];
		while (j < si->width)
			str[j++] = si->fill;
	}
	else
	{
		while (i + si->width > 16)
			buffer[--i] = si->fill;
		while (i < 16)
			str[j++] = buffer[i++];
	}

	return j;
}

int nforml(const sinfo * si, char * str, long v, bool s)
{
	char	buffer[16];

	unsigned long u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	char	i = 16;

	while (u > 0)
	{
		int	c = u % si->base;
		c += c >= 10 ? 'A' - 10 : '0';
		buffer[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 16 - si->precision : 15;

	while (i > digits)
		buffer[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		buffer[--i] = 'X';
		buffer[--i] = '0';
	}

	if (neg)
		buffer[--i] = '-';
	else if (si->sign)
		buffer[--i] = '+';

	char j = 0;
	if (si->left)
	{
		while (i < 16)
			str[j++] = buffer[i++];
		while (j < si->width)
			str[j++] = si->fill;
	}
	else
	{
		while (i + si->width > 16)
			buffer[--i] = si->fill;
		while (i < 16)
			str[j++] = buffer[i++];
	}

	return j;
}

static float fround5[] = {
	0.5e-0, 0.5e-1, 0.5e-2, 0.5e-3, 0.5e-4, 0.5e-5, 0.5e-6
};

int nformf(const sinfo * si, char * str, float f, char type)
{
	char 	* 	sp = str;

	char	d = 0;

	if (f < 0.0)
	{
		f = -f;
		sp[d++] = '-';
	}
	else if (si->sign)
		sp[d++] = '+';		
		
	if (isinf(f))
	{
		sp[d++] = 'I';
		sp[d++] = 'N';
		sp[d++] = 'F';
	}
	else
	{
		int	exp = 0;

		char	fdigits = si->precision != 255 ? si->precision : 6;

		if (f != 0.0)
		{
			while (f >= 1000.0)
			{
				f /= 1000;
				exp += 3;
			}

			while (f < 1.0)
			{
				f *= 1000;
				exp -= 3;
			}

			while (f >= 10.0)
			{
				f /= 10;
				exp ++;
			}
			
		}
		
		char digits = fdigits + 1;
		bool	fexp = type == 'e';

		if (type == 'g')
		{
			if (exp > 3 || exp < 0)
				fexp = true;
		}

		if (!fexp)
		{
			while (exp < 0)
			{
				f /= 10.0;
				exp++;
			}

			digits = fdigits + exp + 1;

			if (digits < 7)
				f += fround5[digits - 1];
			else
				f += fround5[6];

			if (f >= 10.0)
			{
				f /= 10.0;
				fdigits--;
			}		
		}
		else
		{
			if (digits < 7)
				f += fround5[digits - 1];
			else
				f += fround5[6];

			if (f >= 10.0)
			{
				f /= 10.0;
				exp ++;
			}
		}
		

		char	pdigits = digits - fdigits;

		if (digits > 20)
			digits = 20;

		if (pdigits == 0)
			sp[d++] = '0';

		for(char i=0; i<digits; i++)
		{
			if (i == pdigits)
				sp[d++] = '.';
			if (i > 6)
				sp[d++] = '0';
			else
			{
				int c = (int)f;
				f -= (float)c;
				f *= 10.0;
				sp[d++] = c + '0';
			}
		}

		if (fexp)
		{
			sp[d++] = 'E';
			if (exp < 0)
			{
				sp[d++] = '-';
				exp = -exp;
			}
			else
				sp[d++] = '+';
			
			sp[d++] = exp / 10 + '0'; 
			sp[d++] = exp % 10 + '0';		
		}
	}

	if (d < si->width)
	{
		if (si->left)
		{
			for(char i=d; i<si->width; i++)
				sp[i] = ' ';
		}
		else
		{
			for(char i=1; i<=d; i++)
				sp[si->width - i] = sp[d - i];
			for(char i=0; i<si->width-d; i++)
				sp[i] = ' ';
		}
		d = si->width;
	}

	return d;
}

char * sformat(char * buff, const char * fmt, int * fps, bool print)
{
	const char	*	p = fmt;
	char 	* 	bp = buff;
	char		c;
	char		bi = 0;
	sinfo		si;
	
	while (c = *p++)
	{
		if (c == '%')
		{			
			if (bi)
			{
				if (print)
				{
					bp[bi] = 0;
					puts(bp);
				}
				else
					bp += bi;
				bi = 0;
			}
			c = *p++;

			si.base = 10;
			si.width = 0;
			si.precision = 255;
			si.fill = ' ';
			si.sign = false;
			si.left = false;
			si.prefix = false;

			while(true)
			{
				if (c == '+')
					si.sign = true;
				else if (c == '0')
					si.fill = '0';
				else if (c == '#')
					si.prefix = true;
				else if (c == '-')
					si.left = true;
				else					
					break;
				c = *p++;
			}

			if (c >= '0' && c <='9')
			{
				char i = 0;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}
				si.width = i;
			}

			if (c == '.')
			{
				char	i = 0;
				c = *p++;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}		
				si.precision = i;		
			}

			if (c == 'd' || c == p'd')
			{
				bi = nformi(&si, bp, *fps++, true);
			}
			else if (c == 'u' || c == p'u')
			{
				bi = nformi(&si, bp, *fps++, false);
			}
			else if (c == 'x' || c == p'x')
			{
				si.base = 16;
				bi = nformi(&si, bp, *fps++, false);
			}
#ifndef NOLONG
			else if (c == 'l' || c == p'l')
			{
				long l = *(long *)fps;
				fps ++;
				fps ++;

				c = *p++;
				if (c == 'd' || c == p'd')
				{
					bi = nforml(&si, bp, l, true);
				}
				else if (c == 'u' || c == p'u')
				{
					bi = nforml(&si, bp, l, false);
				}
				else if (c == 'x' || c == p'x')
				{
					si.base = 16;
					bi = nforml(&si, bp, l, false);
				}
			}
#endif
#ifndef NOFLOAT
			else if (c == 'f' || c == 'g' || c == 'e' || c == p'f' || c == p'g' || c == p'e')
			{
				bi = nformf(&si, bp, *(float *)fps, c);
				fps ++;
				fps ++;
			}
#endif			
			else if (c == 's' || c == p's')
			{
				char * sp = (char *)*fps++;

				char n = 0;
				if (si.width)
				{
					while (sp[n])
						n++;
				}

				if (!si.left)
				{
					while (n < si.width)
					{
						bp[bi++]  = si.fill;
						n++;
					}
				}

				if (print)
				{
					if (bi)
					{
						bp[bi] = 0;
						puts(bp);
						bi = 0;
					}
					puts(sp);
				}
				else
				{
					while (char c = *sp++)
						bp[bi++] = c;

				}

				if (si.left)
				{
					while (n < si.width)
					{
						bp[bi++] = si.fill;
						n++;
					}						
				}
			}
			else if (c == 'c' || c == p'c')
			{
				bp[bi++] = *fps++;
			}
			else if (c)
			{
				bp[bi++] = c;
			}
		}
		else
		{
			bp[bi++] = c;
			if (bi >= 40)
			{
				if (print)
				{
					bp[bi] = 0;
					puts(bp);
				}
				else
					bp += bi;
				bi = 0;
			}
		}
	}
	bp[bi] = 0;
	if (bi)
	{
		if (print)
			puts(bp);
		else
			bp += bi;
	}

	return bp;
}

void printf(const char * fmt, ...)
{
	char	buff[50];
	sformat(buff, fmt, (int *)&fmt + 1, true);
}

int sprintf(char * str, const char * fmt, ...)
{
	char * d = sformat(str, fmt, (int *)&fmt + 1, false);
	return d - str;
}


static inline bool isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int hexch(char cs)
{
	if (cs >= '0' && cs <= '9')
		return cs - '0';
	else if (cs >= 'a' && cs <= 'f')
		return cs - 'a' + 10;
	else if (cs >= 'A' && cs <= 'F')
		return cs - 'A' + 10;
	else
		return -1;
}

static const char * scanpat(const char * fmt, char * mask)
{
	for(char i=0; i<32; i++)
		mask[i] = 0;
	
	bool	negated = false;
	char fc = *fmt++;
	
	if (fc == '^')
	{
		negated = true;
		fc = *fmt++;
	}
	
	do
	{
		char fd = fc;		
		char nc = *fmt++;
		
		if (nc == '-')
		{
			nc = *fmt;
			if (nc != ']')
			{	
				fmt++;
				fd = nc;
				nc = *fmt++;
			}
		}
		
		while (fc <= fd)
		{
			mask[fc >> 3] |= 0x01 << (fc & 7);
			fc++;
		}
		
		fc = nc;
				
	} while (fc && fc != ']');

	if (negated)
	{
		for(char i=4; i<32; i++)
			mask[i] = ~mask[i];
	}
	
	return fmt;
}

int fpscanf(const char * fmt, int (* ffunc)(void * p), void * fparam, void ** params)
{
	char		fc, cs;
	int			nv = 0;
	unsigned	nch = 0;
	
	cs = ffunc(fparam);
	nch++;

	while (cs > 0 && (fc = *fmt++))
	{
		switch (fc)
		{
			case ' ':
				while (cs > 0 && isspace(cs))
				{
					cs = ffunc(fparam);
					nch++;
				}
				break;
			case '%':		
			{
				int			width = 0x7fff;
				bool		issigned = true;
				bool		ignore = false;
				bool		islong = false;
				unsigned 	base = 10;
				
				fc = *fmt++;
				if (fc == '*')
				{
					ignore = true;
					fc = *fmt++;
				}				
				
				if (fc >= '0' && fc <= '9')
				{
					width = (int)(fc - '0');
					fc = *fmt++;
					while (fc >= '0' && fc <= '9')
					{
						width = width * 10 + (int)(fc - '0');
						fc = *fmt++;
					}
				}
				
				if (fc == 'l' || fc == p'l')
				{
					islong = true;
					fc = *fmt++;
				}				
				
				switch (fc)
				{
					case 'n':
						*(unsigned *)*params = nch;
						params++;					
						nv++;
						break;

					case 'x':
					case p'x':
						base = 16;
					case 'u':
					case p'u':
						issigned = false;
					case 'i':
					case 'd':
					case p'i':
					case p'd':
					{
						bool	sign = false;
						if (cs == '-')
						{
							sign = true;
							cs = ffunc(fparam);
							nch++;
						}
						else if (cs == '+')
						{
							cs = ffunc(fparam);
							nch++;
						}
						
						int cv;
						if ((cv = hexch(cs)) >= 0)
						{	
							cs = ffunc(fparam);
							nch++;

							if (cv == 0 && cs == 'x')
							{
								base = 16;
								cs = ffunc(fparam);
								nch++;
							}
#ifndef NOLONG
							if (islong)
							{
								unsigned long vi = (unsigned long)cv;
								while ((cv = hexch(cs)) >= 0)
								{
									vi = vi * base + (unsigned long)cv;
									cs = ffunc(fparam);
									nch++;
								}
							
								if (!ignore)
								{
									if (sign && issigned)
										*(long *)*params = -(long)vi;
									else
										*(unsigned long *)*params = vi;
										
									params++;					
									nv++;
								}
							}
							else
#endif							
							{
								unsigned	vi = (unsigned)cv;
								while ((cv = hexch(cs)) >= 0)
								{
									vi = vi * base + (unsigned)cv;
									cs = ffunc(fparam);
									nch++;
								}
							
								if (!ignore)
								{
									if (sign && issigned)
										*(int *)*params = -(int)vi;
									else
										*(unsigned *)*params = vi;
										
									params++;					
									nv++;
								}
							}
						}
						else
							return nv;
						
					} break;
#ifndef NOFLOAT
					
					case 'f':
					case 'e':
					case 'g':
					case p'f':
					case p'e':
					case p'g':
					{
						bool	sign = false;
						if (cs == '-')
						{
							sign = true;
							cs = ffunc(fparam);
							nch++;
						}
						else if (cs == '+')
						{
							cs = ffunc(fparam);
							nch++;
						}
							
						if (cs >= '0' && cs <= '9' || cs == '.')
						{	
							float	vf = 0;
							while (cs >= '0' && cs <= '9')
							{
								vf = vf * 10 + (int)(cs - '0');
								cs = ffunc(fparam);
								nch++;
							}

							if (cs == '.')
							{
								float	digits = 1.0;
								cs = ffunc(fparam);
								while (cs >= '0' && cs <= '9')
								{
									vf = vf * 10 + (int)(cs - '0');
									digits *= 10;
									cs = ffunc(fparam);
									nch++;
								}
								vf /= digits;
							}

							char	e = 0;
							bool	eneg = false;								
							
							if (cs == 'e' || cs == 'E')
							{
								cs = ffunc(fparam);
								nch++;
								if (cs == '-')
								{
									eneg = true;
									cs = ffunc(fparam);
									nch++;									
								}
								else if (cs == '+')
								{
									cs = ffunc(fparam);
									nch++;
								}
									
								while (cs >= '0' && cs <= '9')
								{
									e = e * 10 + cs - '0';
									cs = ffunc(fparam);
									nch++;
								}
								
							}
							
							if (!ignore)
							{
								if (e)
								{
									if (eneg)
									{
										while (e > 6)
										{
											vf /= 1000000.0;
											e -= 6;
										}
										vf /= tpow10[e];
									}
									else
									{
										while (e > 6)
										{
											vf *= 1000000.0;
											e -= 6;
										}
										vf *= tpow10[e];
									}
								}

								if (sign)
									*(float *)*params = -vf;
								else
									*(float *)*params = vf;
									
								params++;					
								nv++;
							}
							
						}
						else
							return nv;
						
					} break;
#endif
					case 's':
					case p's':
					{
						char	*	pch = (char *)*params;
						while (width > 0 && cs > 0 && !isspace(cs))
						{
							if (!ignore)
								*pch++ = cs;
							cs = ffunc(fparam);
							nch++;
							width--;
						}
						if (!ignore)
						{
							*pch = 0;
							params++;					
							nv++;
						}
					} break;

					case '[':
					{
						char	pat[32];
						fmt = scanpat(fmt, pat);
						
						char	*	pch = (char *)*params;
						while (width > 0 && (pat[cs >> 3] & (1 << (cs & 7))))
						{
							if (!ignore)
								*pch++ = cs;
							cs = ffunc(fparam);
							nch++;
							width--;
						}
						if (!ignore)
						{
							*pch = 0;
							params++;					
							nv++;
						}
						
					}	break;

					case 'c':
					case p'c':
					{
						char	*	pch = (char *)*params;
						if (!ignore)
						{
							*pch = cs;
							params++;					
							nv++;
						}
						cs = ffunc(fparam);
						nch++;
					} break;
				}
				
			} break;
			
				
			default:
				if (cs == fc)
				{
					cs = ffunc(fparam);
					nch++;
				}
				else
					return nv;					
				break;
		}		
	}
	
	return nv;
}


int sscanf_func(void * fparam)
{
	char ** cp = (char **)fparam;
	char c = **cp;
	if (c)
		(*cp)++;
	return c;
}

int scanf_func(void * fparam)
{
	return getchar();
}

int sscanf(const char * str, const char * fmt, ...)
{
	return fpscanf(fmt, sscanf_func, &str, (void **)((&fmt) + 1));
}

int scanf(const char * fmt, ...)
{
	return fpscanf(fmt, scanf_func, nullptr, (void **)((&fmt) + 1));
}



