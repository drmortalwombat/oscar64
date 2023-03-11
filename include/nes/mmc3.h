#ifndef NES_MMC3_H
#define NES_MMC3_H

#include <c64/types.h>


enum MMC3MPrgMode
{
	MMC3P_8K_LOWER,
	MMC3P_8K_UPPER
};

enum MMC3MChrMode
{
	MMC3C_2K_LOWER,
	MMC3C_2K_HIGHER,
};

enum MMC3BankReg
{
	MMC3B_CHR0,
	MMC3B_CHR1,
	MMC3B_CHR2,
	MMC3B_CHR3,
	MMC3B_CHR4,
	MMC3B_CHR5,

	MMC3B_PRG0,
	MMC3B_PRG1
};

extern char mmc3_shadow;

void mmc3_reset(void);

void mmc3_config(MMC3MPrgMode pmode, MMC3MChrMode cmode);

inline void mmc3_bank(MMC3BankReg reg, char bank);

void mmc3_bank_prg(char bank);

void mmc3_bank_chr0(char bank);

void mmc3_bank_chr1(char bank);

#pragma compile("mmc3.c")

#endif
