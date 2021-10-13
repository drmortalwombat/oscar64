#ifndef C64_CIA
#define C64_CIA

#include "types.h"

struct CIA
{
	byte	pra, prb;
	byte	ddra, ddrb;
	word	ta, tb;
	byte	todt, tods, todm, todh;
	byte	sdr;
	byte	icr;
	byte	cra, crb;
};

#define cia1	(*((CIA *)0xdc00))
#define cia2	(*((CIA *)0xdd00))

#endif



