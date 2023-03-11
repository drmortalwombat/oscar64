#include "mmc1.h"


void mmc1_reset(void)
{
	*(volatile char *)0x8000 = 0x80;
}

void mmc1_config(MMC1Mirror mirror, MMC1MPrgMode pmode, MMC1MChrMode cmode)
{
	char reg = mirror | (pmode << 2) | (cmode << 4);
	*(volatile char *)0x8000 = reg;
	reg >>= 1;
	*(volatile char *)0x8000 = reg;
	reg >>= 1;
	*(volatile char *)0x8000 = reg;
	reg >>= 1;
	*(volatile char *)0x8000 = reg;
	reg >>= 1;
	*(volatile char *)0x8000 = reg;
}

void mmc1_bank_prg(char bank)
{
	*(volatile char *)0xe000 = bank;
	bank >>= 1;
	*(volatile char *)0xe000 = bank;
	bank >>= 1;
	*(volatile char *)0xe000 = bank;
	bank >>= 1;
	*(volatile char *)0xe000 = bank;
	bank >>= 1;
	*(volatile char *)0xe000 = bank;
}

void mmc1_bank_chr0(char bank)
{
	*(volatile char *)0xa000 = bank;
	bank >>= 1;
	*(volatile char *)0xa000 = bank;
	bank >>= 1;
	*(volatile char *)0xa000 = bank;
	bank >>= 1;
	*(volatile char *)0xa000 = bank;
	bank >>= 1;
	*(volatile char *)0xa000 = bank;
}

void mmc1_bank_chr1(char bank)
{
	*(volatile char *)0xc000 = bank;
	bank >>= 1;
	*(volatile char *)0xc000 = bank;
	bank >>= 1;
	*(volatile char *)0xc000 = bank;
	bank >>= 1;
	*(volatile char *)0xc000 = bank;
	bank >>= 1;
	*(volatile char *)0xc000 = bank;
}
