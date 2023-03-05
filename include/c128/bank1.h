#ifndef C128_BANK1_H
#define C128_BANK1_H


#pragma section( bnk1code, 0)

#pragma region( bnk1code, 0xf000, 0xf300, , , {bnk1code}, 0xfc00 )

void bnk1_init(void);

#pragma code(bnk1code)

char bnk1_readb(volatile char * p);

unsigned bnk1_readw(volatile unsigned * p);

unsigned long bnk1_readl(volatile unsigned long * p);

void bnk1_readm(char * dp, volatile char * sp, unsigned size);


void bnk1_writeb(volatile char * p, char b);

void bnk1_writew(volatile unsigned * p, unsigned w);

void bnk1_writel(volatile unsigned long * p, unsigned long l);

void bnk1_writem(volatile char * dp, const char * sp, unsigned size);

#pragma code(code)

#pragma compile("bank1.c")

#endif
