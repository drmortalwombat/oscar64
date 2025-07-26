#include "string.h"

#if 1

char * strcpy(char * dst, const char * src)
{
	__asm 
	{
		lda dst
		sta accu
		lda dst + 1
		sta accu + 1

		ldy #0
	L1:	lda (src), y
		sta (dst), y
		beq W1
		iny
		lda (src), y
		sta (dst), y
		beq W1
		iny
		bne L1
		inc src + 1
		inc dst + 1
		bne L1
	W1:

	}
}

#else
char * strcpy(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	do {} while (*d++ = *s++);
	
	return dst;
}
#endif

#pragma native(strcpy)

char * strncpy(char * dst, const char * src, int n)
{
	char * d = dst;
	const char * s = src;

	do {} while (n-- && (*d++ = *s++));

	// If, after copying the terminating null character from src, count is not reached, 
	// additional null characters are written to dest until the total of count characters 
	// have been written.
	while (n-- > 0)
		*d++ = '\0';

	return dst;
}

#pragma native(strncpy)

#if 1

int strcmp(const char * ptr1, const char * ptr2)
{
	__asm 
	{
		ldy #0
		sty accu + 1
	L1: lda (ptr1), y
		beq W1
		cmp (ptr2), y
		bne W2
		iny
		lda (ptr1), y
		beq W1
		cmp (ptr2), y
		bne W2
		iny
		bne L1
		inc ptr1 + 1
		inc ptr2 + 1
		bne L1

	W1:	cmp (ptr2), y
		beq E

	W2:	bcs W3

		lda #$ff
		sta accu + 1
		bmi E

	W3: lda #$01

	E:
		sta accu
	}
}

#else
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
#endif

#pragma native(strcmp)

int strncmp(const char * ptr1, const char * ptr2, int size)
{
	const char *p = ptr1, *q = ptr2;
	char c = 0, d = 0;

	while (size-- && (c = *p++) == (d = *q++))
	{
		if (!c)
			return 0;
	}

	if (size < 0)
		return 0; // equal up to size

	if (c < d)
		return -1;

	if (c > d)
		return 1;

	return 0; // equal
}

#pragma native(strncmp)

int strlen(const char * str)
{
	const char	*	s = str;
	
	int i = 0;
	while (s[i])
		i++;
	return i;
}

#pragma native(strlen)

char * strcat(char * dst, const char * src)
{
	char * d = dst;
	const char * s = src;

	while (*d)
		d++;
	
	do {} while (*d++ = *s++);
	
	return dst;
}	

#pragma native(strcat)

char * strncat(char * dst, const char * src, int n)
{
	char * d = dst;
	const char * s = src;

	while (*d)
		d++;

	while (1) {
		if (!n--) {
			// The terminating null character is always appended in the end
			if (*d) {
				*d = '\0'; 
			}

			return dst;
		}

		// stopping if the null character is found
		if (!(*d++ = *s++)) {
			return dst;
		}
	}
}

#pragma native(strncat)

char * cpycat(char * dst, const char * src)
{
	while (*dst = *src++)
		dst++;
	
	return dst;	
}

#pragma native(cpycat)

char * strchr( const char * str, int ch )
{
	char * p = (char *)str;

	while (*p != (char)ch)
	{
		if (!*p)
			return nullptr;
		p++;
	}
	return p;
}

char * strrchr(const char * str, int ch)
{
	const char * p = str;
	const char * last = nullptr;

	while (*p)
	{
		if (*p == (char)ch)
			last = p;
		p++;
	}

	// The terminating null character is considered to be a part of the string and can be found if searching for '\0'
	return ch == '\0' ? (char *)p : (char *)last;
}

char * strstr(const char * str, const char * substr)
{
	const char * p = str;
	const char * q = substr;

	if (!*q)
		return (char *)str; // empty substring matches at the start

	while (*p)
	{
		const char * start = p;
		q = substr;

		while (*p && *q && *p == *q)
		{
			p++;
			q++;
		}

		if (!*q) // reached end of substring
			return (char *)start;

		p = start + 1; // move to next character in str
	}

	return nullptr; // substring not found
}

void * memset(void * dst, int value, int size)
{
	__asm
	{
			lda	value

			ldx	size + 1
			beq	_w1
			ldy	#0
	_loop1:
			sta (dst), y
			iny
			bne	_loop1
			inc dst + 1
			dex
			bne	_loop1
	_w1:
			ldy	size
			beq	_w2
	_loop2:
			dey
			sta (dst), y
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
	__asm
	{
			ldx	size + 1
			beq	_w1
			ldy	#0
	_loop1:
			lda (src), y
			sta (dst), y
			iny
			bne	_loop1
			inc src + 1
			inc dst + 1
			dex
			bne	_loop1
	_w1:
			ldy	size
			beq	_w2
			dey
			beq	_w3
	_loop2:
			lda (src), y
			sta (dst), y
			dey
			bne _loop2
	_w3:
			lda (src), y
			sta (dst), y
	_w2:
	}
	return dst;
#if 0

	char	*	d = dst, * s = src;
	while (size--)
		*d++ = *s++;
	return dst;
#endif
}

#pragma native(memcpy)

void * memmove(void * dst, const void * src, int size)
{	
	int	sz = size;
	if (sz > 0)
	{
		char		*	d = (char *)dst;
		const char	*	s = (const char *)src;
		if (d < s)
		{
			do {
				*d++ = *s++;
			} while (--sz);
		}
		else if (d > s)
		{
			d += sz;
			s += sz;
			do {
				*--d = *--s;
			} while (--sz);
		}
	}
	return dst;
}

int memcmp(const void * ptr1, const void * ptr2, int size)
{
	const char	*	p = (const char *)ptr1, * q = (const char *)ptr2;
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

void * memchr(const void * ptr, int ch, int size)
{
	const unsigned char * p = (const unsigned char *)ptr;

	while (size--)
	{
		if (*p == (unsigned char)ch)
			return (void *)p;

		p++;
	}

	return nullptr; // not found
}