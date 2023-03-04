#ifndef NES_NES_H
#define NES_NES_H

#include <c64/types.h>

#define PPU_CTRL_NT_2000	0b00000000
#define PPU_CTRL_NT_2400	0b00000001
#define PPU_CTRL_NT_2800	0b00000010
#define PPU_CTRL_NT_2C00	0b00000011
#define PPU_CTRL_INC_1		0b00000000
#define PPU_CTRL_INC_32		0b00000100
#define PPU_CTRL_SPR_0000	0b00000000
#define PPU_CTRL_SPR_1000	0b00001000
#define PPU_CTRL_BG_0000	0b00000000
#define PPU_CTRL_BG_1000	0b00010000
#define PPU_CTRL_SPR_8X8	0b00000000
#define PPU_CTRL_SPR_8X16	0b00100000
#define PPU_CTRL_NMI		0b10000000

#define PPU_MASK_GREYSCALE	0b00000001
#define PPU_MASK_BG8		0b00000010
#define PPU_MASK_SPR8		0b00000100
#define PPU_MASK_BG_ON		0b00001000
#define PPU_MASK_SPR_ON		0b00010000
#define PPU_MASK_EM_RED		0b00100000
#define PPU_MASK_EM_GREEN	0b01000000
#define PPU_MASK_EM_BLUE	0b10000000

struct PPU
{
	volatile byte	control;
	volatile byte	mask;
	volatile byte	status;
	volatile byte	oamaddr;
	volatile byte	oamdata;
	volatile byte	scroll;
	volatile byte	addr;
	volatile byte	data;	
};

#define ppu	(*((struct PPU *)0x2000))

struct NESIO
{
	volatile byte	sq1_volume;
	volatile byte	sq1_sweep;
	volatile word	sq1_freq;

	volatile byte	sq2_volume;
	volatile byte	sq2_sweep;
	volatile word	sq2_freq;

	volatile byte	tri_volume;
	volatile byte	tri_pad;
	volatile word	tri_freq;

	volatile byte	noise_volume;
	volatile byte	noise_pad;
	volatile word	noise_freq;

	volatile byte	dmc_freq;
	volatile byte	dmc_raw;
	volatile byte	dmc_start;
	volatile byte	dmc_length;

	volatile byte	oamdma;
	volatile byte	channels;
	volatile byte	input[2];
};

#define nesio	(*((struct NESIO *)0x4000))

#pragma compile("nes.c")

#endif

