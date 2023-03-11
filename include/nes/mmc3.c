#include "mmc3.h"

char mmc3_shadow;

void mmc3_reset(void)
{
	mmc3_shadow = 0;
}

void mmc3_config(MMC3MPrgMode pmode, MMC3MChrMode cmode)
{
	mmc3_shadow = (pmode << 6) | (cmode << 7);
	*(volatile char *)0x8000 = mmc3_shadow;	
}

void mmc3_bank(MMC3BankReg reg, char bank)
{
	*(volatile char *)0x8000 = reg | mmc3_shadow;
	*(volatile char *)0x8001 = bank;
}

void mmc3_bank_prg(char bank)
{
	mmc3_bank(MMC3B_PRG0, bank * 2 + 0);
	mmc3_bank(MMC3B_PRG1, bank * 2 + 1);
}

void mmc3_bank_chr0(char bank)
{
	mmc3_bank(MMC3B_CHR0, bank);
}

void mmc3_bank_chr1(char bank)
{
	mmc3_bank(MMC3B_CHR1, bank);
}
