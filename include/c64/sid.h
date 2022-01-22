#ifndef C64_SID_H
#define C64_SID_H

#include "types.h"

#define SID_ATK_2		0x00
#define SID_ATK_8		0x10
#define SID_ATK_16		0x20
#define SID_ATK_24		0x30
#define SID_ATK_38		0x40
#define SID_ATK_56		0x50
#define SID_ATK_68		0x60
#define SID_ATK_80		0x70
#define SID_ATK_100		0x80
#define SID_ATK_250		0x90
#define SID_ATK_500		0xa0
#define SID_ATK_800		0xb0
#define SID_ATK_1000	0xc0
#define SID_ATK_3000	0xd0
#define SID_ATK_5000	0xe0
#define SID_ATK_8000	0xf0

#define SID_DKY_6		0x00
#define SID_DKY_24		0x01
#define SID_DKY_48		0x02
#define SID_DKY_72		0x03
#define SID_DKY_114		0x04
#define SID_DKY_168		0x05
#define SID_DKY_204		0x06
#define SID_DKY_240		0x07
#define SID_DKY_300		0x08
#define SID_DKY_750		0x09
#define SID_DKY_1500	0x0a
#define SID_DKY_2400	0x0b
#define SID_DKY_3000	0x0c
#define SID_DKY_9000	0x0d
#define SID_DKY_15000	0x0e
#define SID_DKY_24000	0x0f

#define SID_CTRL_GATE	0x01
#define SID_CTRL_SYNC	0x02
#define SID_CTRL_RING	0x04
#define SID_CTRL_TEST	0x08
#define SID_CTRL_TRI	0x10
#define SID_CTRL_SAW	0x20
#define SID_CTRL_RECT	0x40
#define SID_CTRL_NOISE	0x80

#define SID_FILTER_1	0x01
#define SID_FILTER_2	0x02
#define SID_FILTER_3	0x04
#define SID_FILTER_X	0x08

#define SID_FMODE_LP	0x10
#define SID_FMODE_BP	0x20
#define SID_FMODE_HP	0x40
#define SID_FMODE_3_OFF	0x80

#define SID_CLOCK_PAL		985248
#define SID_CLOCK_NTSC		1022727

#define SID_CLKSCALE_PAL		1115974UL
#define SID_CLKSCALE_NTSC		1075078UL

#define SID_FREQ_PAL(f)		((unsigned)(((unsigned long)(f) * SID_CLKSCALE_PAL) >> 16))
#define SID_FREQ_NTSC(f)	((unsigned)(((unsigned long)(f) * SID_CLKSCALE_NTSC) >> 16))

struct SID
{
	struct Voice
	{
		volatile unsigned	freq;
		volatile unsigned	pwm;
		volatile byte		ctrl;
		volatile byte		attdec;
		volatile byte		susrel;
	}	voices[3];

	volatile unsigned	ffreq;
	volatile byte		resfilt;
	volatile byte		fmodevol;

	volatile byte		potx;
	volatile byte		poty;
	volatile byte		random;
	volatile byte		env3;
};

#define NOTE_C(o)		(16744U >> (10 - (o)))
#define NOTE_CS(o)		(17740U >> (10 - (o)))
#define NOTE_D(o)		(18794U >> (10 - (o)))
#define NOTE_DS(o)		(19912U >> (10 - (o)))
#define NOTE_E(o)		(21906U >> (10 - (o)))
#define NOTE_F(o)		(22351U >> (10 - (o)))
#define NOTE_FS(o)		(23680U >> (10 - (o)))
#define NOTE_G(o)		(25087U >> (10 - (o)))
#define NOTE_GS(o)		(26580U >> (10 - (o)))
#define NOTE_A(o)		(28160U >> (10 - (o)))
#define NOTE_AS(o)		(29834U >> (10 - (o)))
#define NOTE_B(o)		(31068U >> (10 - (o)))

// reference to the SID chip
#define sid	(*((struct SID *)0xd400))

#pragma compile("sid.c")

#endif
