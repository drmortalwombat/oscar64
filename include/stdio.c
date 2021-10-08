#include "stdio.h"
#include <stdlib.h>

void putchar(char c)
{
	__asm {
		lda c
		bne	w1
		lda #13
	w1:
		jsr	0xffd2
	}
}

char getchar(void)
{
	__asm {
		jsr	0xffcf
		sta	accu
		lda	#0
		sta	accu + 1
	}
}

void puts(const char * str)
{
	__asm {
		ldy	#0
		lda	(str), y
		beq	done
	loop:
		cmp #10
		bne	w1
		lda #13
	w1:
		jsr	0xffd2		
		inc	str
		bne	next
		inc	str + 1
	next:
		ldy	#0
		lda	(str), y
		bne	loop
	done:
	}
}

char * gets(char * str)
{
	__asm {
	loop:
		jsr	0xffcf
		ldy	#0
		cmp	#13
		beq	done
		sta	(str), y
		inc	str
		bne	loop
		inc	srt + 1
		bne	loop
	done:		
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

int nformi(const sinfo * si, char * str, int v, bool s)
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


