#ifndef VIC20_VIC
#define VIC20_VIC

#include <c64/types.h>

struct VICI
{
	volatile byte	hpos;
	volatile byte	vpos;
	volatile byte	ncols;
	volatile byte	nrows;
	volatile byte	beam;
	volatile byte	mempos;

	volatile byte	hlpen, vlpen;
	volatile byte	xpaddle, ypaddle;
	volatile byte	oscfreq[4];
	volatile byte	volcol;
	volatile byte	color;
};

#define vici	(*((struct VICI *)0x9000))

#pragma compile("vic.c")

#endif

