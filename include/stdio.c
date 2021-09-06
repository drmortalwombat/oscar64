#include "stdio.h"
#include <stdlib.h>

void putchar(char c)
{
	__asm {
		ldy	#2
		lda	(fp), y
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

void printf(const char * fmt, ...)
{
	const char	*	p = fmt;
	char		c, buff[14];
	int	*	fps = (int *)&fmt + 1;
	
	while (c = *p++)
	{
		if (c == '%')
		{
			c = *p++;
			if (c == 'd')
			{
				itoa(*fps++, buff, 10);
				puts(buff);
			}
			else if (c == 'x')
			{
				utoa(*fps++, buff, 16);
				puts(buff);
			}
			else if (c == 'f')
			{
				ftoa(*(float *)fps, buff);
				puts(buff);
				fps ++;
				fps ++;
			}
			else if (c == 's')
			{
				puts((char *)*fps++);
			}
			else if (c)
			{
				putchar(c);
			}
		}
		else
			putchar(c);
	}
}

int sprintf(char * str, const char * fmt, ...)
{
	const char	*	p = fmt, * d = str;
	char		c;
	int	*	fps = (int *)&fmt + 1;
	
	while (c = *p++)
	{
		if (c == '%')
		{
			c = *p++;
			if (c == 'd')
			{
				itoa(*fps++, d, 10);
				while (*d)
					d++;
			}
			else if (c == 'x')
			{
				utoa(*fps++, d, 16);
				while (*d)
					d++;
			}
			else if (c == 'f')
			{
				ftoa(*(float *)fps, d);
				fps += 2;
				while (*d)
					d++;				
			}
			else if (c == 's')
			{
				char * s = (char *)*fps++;
				while (c = *s++)
					*d++ = c;
			}
			else if (c)
			{
				*d++ = c;
			}
		}
		else
			*d++ = c;
	}
	*d = 0;
	return d - str;
}


