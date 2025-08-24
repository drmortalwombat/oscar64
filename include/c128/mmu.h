#ifndef C128_MMU_H
#define C128_MMU_H

#include <c64/types.h>

struct MMU
{
	volatile __memmap byte	cr;
	volatile __memmap byte	bank0;
	volatile __memmap byte	bank1;
	volatile __memmap byte	bank14;
	volatile __memmap byte	bankx;
};

struct XMMU
{
	volatile byte	cr;
	volatile byte	pcr[4];
	volatile byte	mcr;
	volatile byte	rcr;
	volatile word	page0;
	volatile word	page1;
	volatile byte	vr;
};

#define mmu		(*((struct MMU *)0xff00))

#define xmmu	(*((struct XMMU *)0xd500))

inline char mmu_set(char cr);

#pragma compile("mmu.c")

#endif

