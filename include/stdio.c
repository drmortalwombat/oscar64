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
	int			width, precision;
	unsigned	base;
	bool		sign, left, prefix;
};

void nformi(const sinfo * si, char * str, int v, bool s)
{
	char	buffer[10];

	unsigned int u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	int	i = 0;
	while (u > 0)
	{
		int	c = u % si->base;
		if (c >= 10)
			c += 'A' - 10;
		else
			c += '0';
		buffer[i++] = c;
		u /= si->base;
	}

	int	digits = si->precision >= 0 ? si->precision : 1;

	while (i < digits)
		buffer[i++] = '0';

	if (si->prefix && si->base == 16)
	{
		buffer[i++] = 'X';
		buffer[i++] = '0';
	}

	if (neg)
		buffer[i++] = '-';
	else if (si->sign)
		buffer[i++] = '+';

	while (i < si->width)
		buffer[i++] = si->fill;

	while (i > 0)
		*str++ = buffer[--i];
	*str++ = 0;
}

void nformf(const sinfo * si, char * str, float f, char type)
{
	int	d = 0;

	if (f < 0.0)
	{
		f = -f;
		str[d++] = '-';
	}
	else if (si->sign)
		str[d++] = '+';		
		
	int	exp = 0;

	int	fdigits = si->precision >= 0 ? si->precision : 6;

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
	
	int digits = fdigits + 1;
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

		float	s = 0.5;
		for(int i=1; i<digits; i++)
			s /= 10.0;
		f += s;
		if (f >= 10.0)
		{
			f /= 10.0;
			fdigits--;
		}		
	}
	else
	{
		float	s = 0.5;
		for(int i=0; i<fdigits; i++)
			s /= 10.0;
		f += s;
		if (f >= 10.0)
		{
			f /= 10.0;
			exp ++;
		}
	}
	
	int	pdigits = digits - fdigits;

	if (digits > 20)
		digits = 20;

	if (pdigits == 0)
		str[d++] = '0';

	for(int i=0; i<digits; i++)
	{
		if (i == pdigits)
			str[d++] = '.';
		int c = (int)f;
		f -= (float)c;
		f *= 10.0;
		str[d++] = c + '0';
	}

	if (fexp)
	{
		str[d++] = 'E';
		if (exp < 0)
		{
			str[d++] = '-';
			exp = -exp;
		}
		else
			str[d++] = '+';
		
		str[d++] = exp / 10 + '0';
		str[d++] = exp % 10 + '0';		
	}
		
	str[d++] = 0;		
	if (d < si->width)
	{
		for(int i=0; i<=d; i++)
			str[si->width - i] = str[d - i];
		for(int i=0; i<si->width-d; i++)
			str[i] = ' '
	}
}

void * sformat(void * data, putstrfn fn, const char * fmt, int * fps)
{
	const char	*	p = fmt;
	char		c, buff[21];
	int			bi = 0;
	sinfo		si;
	
	while (c = *p++)
	{
		if (c == '%')
		{			
			if (bi)
			{
				buff[bi] = 0;
				data = fn(data, buff);
				bi = 0;				
			}
			c = *p++;

			si.base = 10;
			si.width = 1;
			si.precision = -1;
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
				nformi(&si, buff, *fps++, true);
				data = fn(data, buff);
			}
			else if (c == 'u')
			{
				nformi(&si, buff, *fps++, false);
				data = fn(data, buff);
			}
			else if (c == 'x')
			{
				si.base = 16;
				nformi(&si, buff, *fps++, false);
				data = fn(data, buff);
			}
#ifndef NOFLOAT
			else if (c == 'f' || c == 'g' || c == 'e')
			{
				nformf(&si, buff, *(float *)fps, c);
				data = fn(data, buff);
				fps ++;
				fps ++;
			}
#endif			
			else if (c == 's')
			{
				data = fn(data, (char *)*fps++);
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
			if (bi == 10)
			{
				buff[bi] = 0;
				data = fn(data, buff);
				bi = 0;				
			}
		}
	}
	if (bi)
	{
		buff[bi] = 0;
		data = fn(data, buff);
		bi = 0;				
	}

	return data;
}

void printf(const char * fmt, ...)
{
	sformat(nullptr, putstrio, fmt, (int *)&fmt + 1);
}

int sprintf(char * str, const char * fmt, ...)
{
	char * d = (char *)(sformat(str, putstrstr, fmt, (int *)&fmt + 1));
	return d - str;
}


