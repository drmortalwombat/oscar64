#include "vic.h"
#include "cia.h"

void vic_setbank(char bank)
{
	cia2.pra = (cia2.pra & 0xfc) | (bank ^ 0x03);
}
