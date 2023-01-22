#include "cia.h"

byte ciaa_pra_def;

void cia_init(void)
{
	cia1.icr = 0x7f;
	cia2.icr = 0x7f;
	cia1.pra = 0x7f;
	cia1.cra = 0x08;
	cia1.crb = 0x08;
	cia2.cra = 0x08;
	cia2.crb = 0x08;

	cia1.ddrb = 0x00;
	cia2.ddrb = 0x00;
	cia1.ddra = 0xff;
	
	cia2.prb = 0x07;
	cia2.ddra = 0x3f;

	char i0 = cia1.icr;
	char i1 = cia2.icr;

	ciaa_pra_def = 0x7f;	
}
