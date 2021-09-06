#include "string.h"

char * strcpy(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	do {} while (*d++ = *s++);
	
	return dst;
}

int strcmp(const char * ptr1, const char * ptr2)
{
	const char	*	p = ptr1, * q = ptr2;
	char		c, d;
	while ((c = *p++) == (d = *q++))
	{
		if (!c)
			return 0;
	}
	if (c < d)
		return -1;
	else
		return 1;
}

int strlen(const char * str)
{
	const char	*	s = str;
	
	int i = 0;
	while (s[i])
		i++;
	return i;
}

char * strcat(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	while (*d)
		d++;
	
	do {} while (*d++ = *s++);
	
	return dst;
}	

void * memset(void * dst, int value, int size)
{
	__asm
	{
			ldy	#dst
			lda	(fp), y
			sta $1f
			iny
			lda	(fp), y
			sta $20

			ldy #size
			lda	(fp), y
			sta	$1b	
			iny
			lda	(fp), y
			sta	$1c

			ldy	#value
			lda	(fp), y

			ldx	$1c
			beq	_w1
			ldy	#0
	_loop1:
			sta ($1f), y
			iny
			bne	_loop1
			inc $20
			dex
			bne	_loop1
	_w1:
			ldy	$1b
			beq	_w2
	_loop2:
			dey
			sta ($1f), y
			bne _loop2
	_w2:
	}
	return dst;
}
	

void * memclr(void * dst, int size)
{
	char	*	d = dst;
	while (size--)
		*d++ = 0;
	return dst;
}	

void * memcpy(void * dst, const void * src, int size)
{
	char	*	d = dst, * s = src;
	while (size--)
		*d++ = *s++;
	return dst;
}

void * memmove(void * dst, const void * src, int size)
{
	char	*	d = dst, * s = src;
	if (d < s)
	{
		while (size--)
			*d++ = *s++;
	}
	else if (d > s)
	{
		d += size;
		s += size;
		while (size--)
			*--d = *--s;
	}	
	return dst;
}

int memcmp(const void * ptr1, const void * ptr2, int size)
{
	char	*	p = ptr1, * q = ptr2;
	char		c, d;

	while (size--)
	{
		c = *p++;
		d = *q++;
		if (c < d)
			return -1;
		else if (c > d)
			return 1;
	}
	
	return 0;
}
	


