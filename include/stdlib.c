#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "math.h"
#include "crt.h"

void itoa(int n, char * s, unsigned radix)
{
	bool neg = n < 0;
	unsigned un = neg ? -n : n;
	
	char	i = 0;
    do {
		unsigned	d = un % radix;
		if (d < 10)
			d += '0';
		else
			d += 'A' - 10;
		s[i++] = d;
    } while ((un /= radix) > 0);

	if (neg)
	{
		s[i++] = '-';
	}
	s[i] = 0;
	char	j = 0;
	while (j + 1 < i)
	{
		char c = s[j];
		s[j++] = s[--i];
		s[i] = c;
	}
}

void utoa(unsigned int n, char * s, unsigned radix)
{	
	char	i = 0;
    do {
		unsigned int	d = n % radix;
		if (d < 10)
			d += '0';
		else
			d += 'A' - 10;
		s[i++] = d;
    } while ((n /= radix) > 0);

	s[i] = 0;
	char	j = 0;
	while ((char)(j + 1) < i)
	{
		char c = s[j];
		s[j++] = s[--i];
		s[i] = c;
	}
}

void ltoa(long n, char * s, unsigned radix)
{
	bool neg = n < 0;
	if (neg)
	{
		n = - n;
	}
	
	char	i = 0;
    do {
		int	d = n % radix;
		if (d < 10)
			d += '0';
		else
			d += 'A' - 10;
		s[i++] = d;
    } while ((n /= radix) > 0);

	if (neg)
	{
		s[i++] = '-';
	}
	s[i] = 0;
	char	j = 0;
	while (j + 1 < i)
	{
		char c = s[j];
		s[j++] = s[--i];
		s[i] = c;
	}
}

void ultoa(unsigned long n, char * s, unsigned radix)
{	
	char	i = 0;
    do {
		unsigned int	d = n % radix;
		if (d < 10)
			d += '0';
		else
			d += 'A' - 10;
		s[i++] = d;
    } while ((n /= radix) > 0);

	s[i] = 0;
	char	j = 0;
	while (j + 1 < i)
	{
		char c = s[j];
		s[j++] = s[--i];
		s[i] = c;
	}
}


void ftoa(float f, char * s)
{
	if (f < 0.0)
	{
		f = -f;
		*s++ = '-';
	}
		
	int	digits = 0;
	while (f >= 1000.0)
	{
		f /= 1000;
		digits += 3;
	}
		
	if (f != 0.0)
	{
		while (f < 1.0)
		{
			f *= 1000;
			digits -= 3;
		}

		while (f >= 10.0)
		{
			f /= 10;
			digits ++;
		}
		
		f += 0.0000005;
		if (f >= 10.0)
		{
			f /= 10.0;
			digits ++;
		}
	}
	
	int	exp = 0;
	if (digits < 0)
	{
		exp = digits;
		digits = 0;
	}
	else if (digits > 6)
	{
		exp = digits;
		digits = 0;
	}
	
	for(int i=0; i<7; i++)
	{
		int c = (int)f;
		f -= (float)c;
		f *= 10.0;
		*s++ = c + '0';
		if (i == digits)
			*s++ = '.';
	}
	if (exp)
	{
		*s++ = 'E';
		if (exp < 0)
		{
			*s++ = '-';
			exp = -exp;
		}
		else
			*s++ = '+';
		
		if (exp >= 10)
		{
			*s++ = exp / 10 + '0';
			exp %= 10;
		}
		*s++ = exp + '0';		
	}
		
	*s++= 0;		
}

int atoi(const char * s)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;
	
	bool neg = false;
	if (c == '-')
	{
		neg = true;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;
	
	int	v = 0;
	while (c >= '0' && c <= '9')
	{
		v = v * 10 + (c - '0');
		c = *s++;
	}
	
	if (neg)
		v = -v;
	
	return v;		
}

const float tpow10[7] = {1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};

float atof(const char * s)
{
	return strtof(s, nullptr);
}

float strtof(const char *s, const char **endp)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;
	
	bool neg = false;
	if (c == '-')
	{
		neg = true;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;
	
	float	v = 0;
	while (c >= '0' && c <= '9')
	{
		v = v * 10 + (c - '0');
		c = *s++;
	}

	if (c == '.')
	{
		float	d = 1.0;
		c = *s++;
		while (c >= '0' && c <= '9')
		{
			v = v * 10 + (c - '0');
			d = d * 10;
			c = *s++;
		}

		v /= d;
	}

	if (c == 'e' || c == 'E')
	{
		c = *s++;
		bool	eneg = false;
		if (c == '-')
		{
			eneg = true;
			c = *s++;
		}
		else if (c == '+')
			c = *s++;

		int	e = 0;
		while (c >= '0' && c <= '9')
		{
			e = e * 10 + (c - '0');
			c = *s++;
		}

		if (eneg)
		{
			while (e > 6)
			{
				v /= 1000000.0;
				e -= 6;
			}
			v /= tpow10[e];
		}
		else
		{
			while (e > 6)
			{
				v *= 1000000.0;
				e -= 6;
			}
			v *= tpow10[e];
		}
		
	}
	
	if (neg)
		v = -v;
	
	if (endp)
		*endp = (char *)s;

	return v;		
}

int strtoi(const char *s, const char **endp, char base)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;
	
	bool neg = false;
	if (c == '-')
	{
		neg = true;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;

	if (c == '0')
	{
		c = *s++;
		if (c == 'x' || c == 'X')
		{
			base = 16;
			c = *s++;
		}
		else if (base == 0)
			base = 8;		
	} else if (base == 0)
		base = 10;
	
	int	v = 0;
	for(;;)
	{
		if (c >= '0' && c <= '9')
			v = v * base + (c - '0');
		else if (c >= 'a' && c <= 'f')
			v = v * base + (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v = v * base + (c - 'A' + 10);
		else
			break;

		c = *s++;
	}
	
	if (neg)
		v = -v;

	if (endp)
		*endp = (char *)s;
	
	return v;
}

unsigned strtou(const char *s, const char **endp, char base)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;

	if (c == '0')
	{
		c = *s++;
		if (c == 'x' || c == 'X')
		{
			base = 16;
			c = *s++;
		}
		else if (base == 0)
			base = 8;		
	} else if (base == 0)
		base = 10;
		
	unsigned	v = 0;
	for(;;)
	{
		if (c >= '0' && c <= '9')
			v = v * base + (c - '0');
		else if (c >= 'a' && c <= 'f')
			v = v * base + (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v = v * base + (c - 'A' + 10);
		else
			break;

		c = *s++;
	}
	
	if (endp)
		*endp = (char *)s;
	
	return v;
}

long strtol(const char *s, const char **endp, char base)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;
	
	bool neg = false;
	if (c == '-')
	{
		neg = true;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;

	if (c == '0')
	{
		c = *s++;
		if (c == 'x' || c == 'X')
		{
			base = 16;
			c = *s++;
		}
		else if (base == 0)
			base = 8;		
	} else if (base == 0)
		base = 10;
	
	long	v = 0;
	for(;;)
	{
		if (c >= '0' && c <= '9')
			v = v * base + (c - '0');
		else if (c >= 'a' && c <= 'f')
			v = v * base + (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v = v * base + (c - 'A' + 10);
		else
			break;

		c = *s++;
	}
	
	if (neg)
		v = -v;

	if (endp)
		*endp = (char *)s;
	
	return v;
}

unsigned long strtoul(const char *s, const char **endp, char base)
{
	char	c;
	while ((c = *s++) <= ' ')
		if (!c) return 0;
		
	if (c == '0')
	{
		c = *s++;
		if (c == 'x' || c == 'X')
		{
			base = 16;
			c = *s++;
		}
		else if (base == 0)
			base = 8;		
	} else if (base == 0)
		base = 10;

	unsigned long v = 0;
	for(;;)
	{
		if (c >= '0' && c <= '9')
			v = v * base + (c - '0');
		else if (c >= 'a' && c <= 'f')
			v = v * base + (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v = v * base + (c - 'A' + 10);
		else
			break;

		c = *s++;
	}
	
	if (endp)
		*endp = (char *)s;
	
	return v;
}

int abs(int n)
{
	return n < 0 ? - n : n;
}

long labs(long n)
{
	return n < 0 ? - n : n;	
}

void exit(int status)
{
	__asm
	{
		lda	status
		sta	accu + 0
		lda	status + 1
		sta	accu + 1
		ldx	spentry
		txs
		lda	#$4c
		sta	$54
		lda #0
		sta $13
	}
}

void abort(void)
{
	exit(-1);
}


extern struct Heap {
	Heap	*	next, * end;
}	HeapNode;

unsigned heapfree(void)
{
	unsigned	avail = 0;
	Heap	*	h = HeapNode.next;
	while (h)
	{
		avail += (h->end - h) * sizeof(Heap);
		h = h->next;
	}
	return avail;
}

#if 0
struct Heap {q
	unsigned int	size;
	Heap		*	next;
}	*	freeHeap;

bool	freeHeapInit = false;

void HeapStart, HeapEnd;

#pragma section(heap, 0x0000, HeapStart, HeapEnd)


void * malloc(unsigned int size)
{
	size = (size + 7) & ~3;
	if (!freeHeapInit)
	{
		freeHeap = (Heap *)&HeapStart;
		freeHeap->next = nullptr;
		freeHeap->size = (unsigned int)&HeapEnd - (unsigned int)&HeapStart;
		freeHeapInit = true;
	}
	
	Heap	*	pheap = nullptr, * heap = freeHeap;
	while (heap)
	{
		if (size <= heap->size)
		{
			if (size == heap->size)
			{
				if (pheap)
					pheap->next = heap->next;
				else
					freeHeap = heap->next;				
			}
			else
			{
				Heap	*	nheap = (Heap *)((int)heap + size);
				nheap->size = heap->size - size;
				nheap->next = heap->next;
				if (pheap)
					pheap->next = nheap;
				else
					freeHeap = nheap;
				heap->size = size;
			}
			
			return (void *)((int)heap + 2);
		}
		pheap = heap;
		heap = heap->next;
	}
		
	return nullptr;	
}

void free(void * ptr)
{
	if (!ptr)
		return;

	Heap	*	fheap = (Heap *)((int)ptr - 2);
	Heap	*	eheap = (Heap *)((int)ptr - 2 + fheap->size);
	
	if (freeHeap)
	{
		if (eheap == freeHeap)
		{
			fheap->size += freeHeap->size;
			fheap->next = freeHeap->next;
			freeHeap = fheap;
		}
		else if (eheap < freeHeap)
		{
			fheap->next = freeHeap;
			freeHeap = fheap;
		}
		else
		{
			Heap	*	pheap = freeHeap;
			while (pheap->next && pheap->next < fheap)
				pheap = pheap->next;
			Heap	*	nheap = (Heap *)((int)pheap + pheap->size);
			
			if (nheap == fheap)
			{
				pheap->size += fheap->size;
				if (pheap->next == eheap)
				{
					pheap->size += pheap->next->size;
					pheap->next = pheap->next->next;
				}
			}			
			else if (pheap->next == eheap)
			{
				fheap->next = pheap->next->next;
				fheap->size += pheap->next->size;
				pheap->next = fheap;
			}
			else
			{
				fheap->next = pheap->next;
				pheap->next = fheap;
			}
		}
	}
	else		
	{
		freeHeap = fheap;
		freeHeap->next = nullptr;
	}
}
#endif

void * calloc(int num, int size)
{
	size *= num;
	void * p = malloc(size);
	if (p)
		memclr(p, size);
	return p;
}

void * realloc(void * ptr, unsigned size)
{
	if (ptr)
	{
#ifdef HEAPCHECK
		Heap	*	pheap = (Heap *)((char *)ptr - 6);
		Heap	*	eheap = pheap->next;

		unsigned	psize = (char *)eheap - (char *)pheap;

		void * nptr = malloc(size);
		memcpy(nptr, ptr, psize - 6);
		free(ptr);
		return nptr;
#else
		unsigned	nsize = (size + 5) & ~3;

		// Get heap info
		Heap	*	pheap = (Heap *)((char *)ptr - 2);
		Heap	*	eheap = pheap->next;

		unsigned	psize = (char *)eheap - (char *)pheap;

		Heap	*	h = HeapNode.next;
		Heap	*	hp = &HeapNode;
		while (h && h < pheap)
		{
			hp = h;
			h = h->next;
		}		

		if (nsize <= psize)
		{
			// check if we should free some memory
			if (nsize + sizeof(HeapNode) < psize)
			{
				Heap	*	nheap = (Heap *)((char *)pheap + nsize);
				pheap->next = nheap;

				if (h == eheap)
				{
					nheap->end = h->end;
					nheap->next = h->next;
				}
				else 
				{
					nheap->end = eheap;
					nheap->next = h;
				}

				hp->next = nheap;
			}

			return ptr;
		}
		else if (h == eheap)
		{
			// Free space after this

			// Check if enough space if extending
			unsigned	xsize = (char *)h->end - (char *)pheap;
			if (xsize >= nsize)
			{
				if (xsize > nsize + sizeof(HeapNode))
				{
					Heap	*	nheap = (Heap *)((char *)pheap + nsize);
					pheap->next = nheap;
					nheap->end = h->end;
					nheap->next = h->next;
					hp->next = nheap;
				}
				else
				{
					pheap->next = h->end;
					hp->next = h->next;
				}

				return ptr;
			}
		}

		void * nptr = malloc(size);
		memcpy(nptr, ptr, psize - 2);
		free(ptr);
		return nptr;
#endif		
	}
	else
		return malloc(size);
}

static unsigned seed = 31232;
static unsigned seedl = 321432142412;

unsigned int rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
	return seed;
}

void srand(unsigned int s)
{
	seed = s;
}

unsigned long lrand(void)
{
	seedl ^= seedl << 13;
	seedl ^= seedl >> 17;
	seedl ^= seedl << 5;
	return seedl;
}

void lsrand(unsigned long s)
{
	seedl = s;
}
