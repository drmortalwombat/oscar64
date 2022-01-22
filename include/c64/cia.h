#ifndef C64_CIA
#define C64_CIA

#include "types.h"

struct CIA
{
	volatile byte	pra, prb;
	byte	ddra, ddrb;
	word	ta, tb;
	byte	todt, tods, todm, todh;
	byte	sdr;
	byte	icr;
	byte	cra, crb;
};

#define cia1	(*((struct CIA *)0xdc00))
#define cia2	(*((struct CIA *)0xdd00))

void cia_init(void);

#pragma compile("cia.c")

#endif



