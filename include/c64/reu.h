#ifndef C64_REU_H
#define C64_REU_H

#include "types.h"

#define REU_STAT_IRQ		0x80
#define REU_STAT_EOB		0x40
#define REU_STAT_FAULT		0x20
#define REU_STAT_SIZE		0x10
#define REU_STAT_VERSION	0x0f

#define REU_CTRL_FIXL		0x80
#define REU_CTRL_FIXR		0x40
#define REU_CTRL_INCL		0x00
#define REU_CTRL_INCR		0x00

#define REU_IRQ_ENABLE		0x80
#define REU_IRQ_EOB			0x40
#define REU_IRQ_FAULT		0x20

#define REU_CMD_EXEC		0x80
#define REU_CMD_AUTO		0x20
#define REU_CMD_FF00		0x10
#define REU_CMD_STORE		0x00
#define REU_CMD_LOAD		0x01
#define REU_CMD_SWAP		0x02
#define REU_CMD_VERIFY		0x03

struct REU
{
	volatile byte	status;
	volatile byte	cmd;

	volatile word	laddr;
	volatile word	raddr;
	volatile byte	rbank;

	volatile word	length;

	volatile byte	irqmask;
	volatile byte	ctrl;
};


#define reu 	(*((struct REU *)0xdf00))


#pragma compile("reu.c")

#endif
