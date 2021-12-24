#include "stdio.h"
#include "conio.h"
#include "stdlib.h"

__asm putpch
{
		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp #10
		bne	w1
		lda #13
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #65
		bcc w3
		cmp	#123
		bcs	w3
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
		cpx #IOCHM_PETSCII_2
		beq	w3
		and #$df
	w3:
		jmp	0xffd2	
}

__asm getpch
{
		jsr	0xffcf

		ldx	giocharmap
		cpx	#IOCHM_ASCII
		bcc	w3

		cmp	#13
		bne	w1
		lda #10
	w1:
		cpx	#IOCHM_PETSCII_1
		bcc	w3

		cmp #65
		bcc w3
		cmp	#123
		bcs	w3
		cmp	#97
		bcs	w2
		cmp #91
		bcs	w3
	w2:
		eor	#$20
	w3:
}

void putchar(char c)
{
	__asm {
		lda c
		jmp	putpch
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

inline int nformi(const sinfo * si, char * str, int v, bool s)
{
	char * sp = str;

	unsigned int u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	char	i = 10;
	while (u > 0)
	{
		int	c = u % si->base;
		if (c >= 10)
			c += 'A' - 10;
		else
			c += '0';
		sp[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 10 - si->precision : 9;

	while (i > digits)
		sp[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		sp[--i] = 'X';
		sp[--i] = '0';
	}

	if (neg)
		sp[--i] = '-';
	else if (si->sign)
		sp[--i] = '+';

	while (i > 10 - si->width)
		sp[--i] = si->fill;

	char j = 0;
	while (i < 10)
		sp[j++] = sp[i++];

	return j;
}

int nforml(const sinfo * si, char * str, long v, bool s)
{
	char * sp = str;

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
		if (c >= 10)
			c += 'A' - 10;
		else
			c += '0';
		sp[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 16 - si->precision : 15;

	while (i > digits)
		sp[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		sp[--i] = 'X';
		sp[--i] = '0';
	}

	if (neg)
		sp[--i] = '-';
	else if (si->sign)
		sp[--i] = '+';

	while (i > 16 - si->width)
		sp[--i] = si->fill;

	char j = 0;
	while (i < 16)
		sp[j++] = sp[i++];

	return j;
}

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

			float	r = 0.5;
			for(char i=1; i<digits; i++)
				r /= 10.0;
			f += r;
			if (f >= 10.0)
			{
				f /= 10.0;
				fdigits--;
			}		
		}
		else
		{
			float	r = 0.5;
			for(char i=0; i<fdigits; i++)
				r /= 10.0;
			f += r;
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
			int c = (int)f;
			f -= (float)c;
			f *= 10.0;
			sp[d++] = c + '0';
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
		for(char i=1; i<=d; i++)
			sp[si->width - i] = sp[d - i];
		for(char i=0; i<si->width-d; i++)
			sp[i] = ' ';
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
			si.width = 1;
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
				else					
					break;
				c = *p++;
			}

			if (c >= '0' && c <='9')
			{
				int i = 0;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}
				si.width = i;
			}

			if (c == '.')
			{
				int	i = 0;
				c = *p++;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}		
				si.precision = i;		
			}

			if (c == 'd')
			{
				bi = nformi(&si, bp, *fps++, true);
			}
			else if (c == 'u')
			{
				bi = nformi(&si, bp, *fps++, false);
			}
			else if (c == 'x')
			{
				si.base = 16;
				bi = nformi(&si, bp, *fps++, false);
			}
#ifndef NOLONG
			else if (c == 'l')
			{
				long l = *(long *)fps;
				fps ++;
				fps ++;

				c = *p++;
				if (c == 'd')
				{
					bi = nforml(&si, bp, l, true);
				}
				else if (c == 'u')
				{
					bi = nforml(&si, bp, l, false);
				}
				else if (c == 'x')
				{
					si.base = 16;
					bi = nforml(&si, bp, l, false);
				}
			}
#endif
#ifndef NOFLOAT
			else if (c == 'f' || c == 'g' || c == 'e')
			{
				bi = nformf(&si, bp, *(float *)fps, c);
				fps ++;
				fps ++;
			}
#endif			
			else if (c == 's')
			{
				char * sp = (char *)*fps++;
				if (print)
					puts(sp);
				else
				{
					while (char c = *sp++)
						*bp++ = c;
				}
			}
			else if (c == 'c')
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

	if (fc == ']')
		fmt++;

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
	unsigned	nch = 0
	
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
				
				if (fc == 'l')
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
						base = 16;
					case 'u':
						issigned = false;
					case 'i':
					case 'd':
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

int sscanf(const char * fmt, const char * str, ...)
{
	return fpscanf(fmt, sscanf_func, &str, (void **)((&str) + 1));
}

int scanf(const char * fmt, ...)
{
	return fpscanf(fmt, scanf_func, nullptr, (void **)((&fmt) + 1));
}



