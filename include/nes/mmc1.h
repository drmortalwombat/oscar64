#ifndef NES_MMC1_H
#define NES_MMC1_H

#include <c64/types.h>

enum MMC1Mirror
{
	MMC1M_LOWER,
	MMC1M_UPPER,
	MMC1M_VERTICAL,
	MMC1M_HORIZONTAL
};

enum MMC1MPrgMode
{
	MMC1P_32K,
	MMC1P_32Kx,
	MMC1P_16K_UPPER,
	MMC1P_16K_LOWER
};

enum MMC1MChrMode
{
	MMC1C_8K,
	MMC1C_4Kx,
};

void mmc1_reset(void);

void mmc1_config(MMC1Mirror mirror, MMC1MPrgMode pmode, MMC1MChrMode cmode);

void mmc1_bank_prg(char bank);

void mmc1_bank_chr0(char bank);

void mmc1_bank_chr1(char bank);

#pragma compile("mmc1.c")

#endif
