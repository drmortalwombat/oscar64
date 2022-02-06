#ifndef C64_CIA
#define C64_CIA

#include "types.h"

struct CIA
{
	volatile byte	pra, prb;
	volatile byte	ddra, ddrb;
	volatile word	ta, tb;
	volatile byte	todt, tods, todm, todh;
	volatile byte	sdr;
	volatile byte	icr;
	volatile byte	cra, crb;
};

#define cia1	(*((struct CIA *)0xdc00))
#define cia2	(*((struct CIA *)0xdd00))

void cia_init(void);

#pragma compile("cia.c")

#endif



