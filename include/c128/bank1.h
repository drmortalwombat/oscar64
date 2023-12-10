#ifndef C128_BANK1_H
#define C128_BANK1_H


#pragma section( bnk1code, 0)

#pragma region( bnk1code, 0xf000, 0xf300, , , {bnk1code}, 0xfc00 )

void bnk1_init(void);

#pragma code(bnk1code)

__noinline char bnk1_readb(volatile char * p);

__noinline unsigned bnk1_readw(volatile unsigned * p);

__noinline unsigned long bnk1_readl(volatile unsigned long * p);

__noinline void bnk1_readm(char * dp, volatile char * sp, unsigned size);


__noinline void bnk1_writeb(volatile char * p, char b);

__noinline void bnk1_writew(volatile unsigned * p, unsigned w);

__noinline void bnk1_writel(volatile unsigned long * p, unsigned long l);

__noinline void bnk1_writem(volatile char * dp, const char * sp, unsigned size);

#pragma code(code)

#pragma compile("bank1.c")

#endif
