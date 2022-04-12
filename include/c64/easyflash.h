#ifndef C64_EASYFLASH_H
#define C64_EASYFLASH_H

#include "types.h"

struct EasyFlash
{
	volatile byte	bank;
	byte			pad1;
	volatile byte	control;
};

#define EFCTRL_GAME		0x01
#define EFCTRL_EXROM	0x02
#define EFCTRL_MODE		0x04
#define EFCTRL_LED		0x80


#define eflash	(*(EasyFlash *)0xde00)


#endif

