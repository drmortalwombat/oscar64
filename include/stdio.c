#include "stdio.h"
#include <stdlib.h>

void putchar(char c)
{
	__asm {
		ldy	#2
		lda	(fp), y
		cmp #10
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
		sta	0x1b
		lda	#0
		sta	0x1c
	}
}

void puts(const char * str)
{
	__asm {
		ldy	#2
		lda	(fp), y
		sta 0x02
		iny
		lda	(fp), y
		sta 0x03
		ldy	#0
		lda	(0x02), y
		beq	done
	loop:
		cmp #10
		bne	w1
		lda #13
	w1:
		jsr	0xffd2		
		inc	0x02
		bne	next
		inc	0x03
	next:
		ldy	#0
		lda	(0x02), y
		bne	loop
	done:
	}
}

char * gets(char * str)
{
	__asm {
		ldy	#2
		lda	(fp), y
		sta 0x02
		iny
		lda	(fp), y
		sta 0x03
	loop:
		jsr	0xffcf
		ldy	#0
		cmp	#13
		beq	done
		sta	(0x02), y
		inc	0x02
		bne	loop
		inc	0x03
		bne	loop
	done:		
		lda	#0
		sta	(0x02), y
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

	int	digits = si->precision != 255 ? 10 - si->precision : 9;

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
	char		c;
	int			bi = 0;
	sinfo		si;
	
	while (c = *p++)
	{
		if (c == '%')
		{			
			if (bi)
			{
				if (print)
				{
					buff[bi] = 0;
					puts(buff);
				}
				else
					buff += bi;
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
				bi = nformi(&si, buff, *fps++, true);
			}
			else if (c == 'u')
			{
				bi = nformi(&si, buff, *fps++, false);
			}
			else if (c == 'x')
			{
				si.base = 16;
				bi = nformi(&si, buff, *fps++, false);
			}
#ifndef NOFLOAT
			else if (c == 'f' || c == 'g' || c == 'e')
			{
				bi = nformf(&si, buff, *(float *)fps, c);
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
						*buff++ = c;
				}
			}
			else if (c == 'c')
			{
				buff[bi++] = *fps++;
			}
			else if (c)
			{
				buff[bi++] = c;
			}
		}
		else
		{
			buff[bi++] = c;
			if (bi >= 10)
			{
				if (print)
				{
					buff[bi] = 0;
					puts(buff);
				}
				else
					buff += bi;
				bi = 0;
			}
		}
	}
	buff[bi] = 0;
	if (bi)
	{
		if (print)
			puts(buff);
		else
			buff += bi;
	}

	return buff;
}

void printf(const char * fmt, ...)
{
	char	buff[40];
	sformat(buff, fmt, (int *)&fmt + 1, true);
}

int sprintf(char * str, const char * fmt, ...)
{
	char * d = sformat(str, fmt, (int *)&fmt + 1, false);
	return d - str;
}


