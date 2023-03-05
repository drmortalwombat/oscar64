#include "bank1.h"
#include "mmu.h"
#include <string.h>

void bnk1_init(void)
{
	mmu.cr = 0x3e;
	memcpy((char *)0xfc00, (char *)0xf000, 0x0300);
	xmmu.rcr |= 0x0c;
	mmu.cr = 0x3f;
}

#pragma code(bnk1code)

char bnk1_readb(volatile char * p)
{
	mmu.bank1 = 0;
	char c = *p;
	mmu.bank0 = 0;
	return c;
}

unsigned bnk1_readw(volatile unsigned * p)
{
	mmu.bank1 = 0;
	unsigned w = *p;
	mmu.bank0 = 1;
	return w;
}

unsigned long bnk1_readl(volatile unsigned long * p)
{
	mmu.bank1 = 0;
	unsigned long l = *p;
	mmu.bank0 = 3;
	return l;
}

void bnk1_readm(char * dp, volatile char * sp, unsigned size)
{
	while (size > 0)
	{
		mmu.bank1 = 0;
		char c = * sp++;
		mmu.bank0 = c;
		*dp++ = c;
		size--;
	}
}

void bnk1_writeb(volatile char * p, char b)
{
	mmu.bank1 = b;
	*p = b;
	mmu.bank0 = b;
}

void bnk1_writew(volatile unsigned * p, unsigned w)
{
	mmu.bank1 = w;
	*p = w;
	mmu.bank0 = w;
}

void bnk1_writel(volatile unsigned long * p, unsigned long l)
{
	mmu.bank1 = l;
	*p = l;
	mmu.bank0 = l;
}

void bnk1_writem(volatile char * dp, const char * sp, unsigned size)
{
	while (size > 0)
	{
		char c = * sp++;
		mmu.bank1 = c;
		*dp++ = c;
		mmu.bank0 = c;
		size--;
	}	
}

#pragma code(code)
