#ifndef PLUS4_TED_H
#define PLUS4_TED_H

#include <c64/types.h>

#define TED_CTRL1_RSEL	0x08
#define TED_CTRL1_DEN	0x10
#define TED_CTRL1_BMM	0x20
#define TED_CTRL1_ECM	0x40

#define TED_CTRL2_CSEL	0x08
#define TED_CTRL2_MCM	0x10
#define TED_CTRL2_RES	0x20
#define TED_CTRL2_NTSC	0x40
#define TED_CTRL2_INV	0x80

#define TED_INTR_RST	0x02
#define TED_INTR_LPEN	0x04
#define TED_INTR_CNT1	0x08
#define TED_INTR_CNT2	0x10
#define TED_INTR_CNT3	0x20
#define TED_INTR_IRQ	0x80

#define TED_SND_SQUARE1	0x10
#define TES_SND_SQUARE2	0x20
#define TES_SND_NOISE2	0x40
#define TES_SND_DA		0x80

#define TES_CHAR_ROM	0x04

struct TED
{
	volatile word	timer0;
	volatile word	timer1;

	volatile word	timer2;
	volatile byte	ctrl1;
	volatile byte	ctrl2;

	volatile byte	keybin;
	volatile byte	intr_ctrl;
	volatile byte	intr_enable;
	volatile byte	raster;

	volatile word	cursor_pos;
	volatile byte	sound1_low;
	volatile byte	sound2_low;

	volatile byte	sound2_high;
	volatile byte	sound_ctrl;
	volatile byte	sound1_high;
	volatile byte	char_ptr;

	volatile byte	vid_ptr;
	volatile byte	color_back;
	volatile byte	color_back1;
	volatile byte	color_back2;

	volatile byte	color_back3;
	volatile byte	color_border;
	volatile byte	char_pos_high;
	volatile byte	char_pos_low;

	volatile byte	vscan_high;
	volatile byte	vscan_low;
	volatile byte	hscan;
	volatile byte	flash;
};

enum TedMode
{
	TEDM_TEXT,
	TEDM_TEXT_MC,
	TEDM_TEXT_ECM,
	TEDM_HIRES,
	TEDM_HIRES_MC
};

// set the display mode and base address. This will also
// adapt the bank.
void ted_setmode(TedMode mode, char * text, char * font);

// wait for the beam to reach the bottom of the visual area
inline void ted_waitBottom(void);

// wait for the beam to reach the top of the frame
inline void ted_waitTop(void);

// wait for the top of the frame and then for the bottom of the visual area
inline void ted_waitFrame(void);

// wait for a specific raster line
void ted_waitLine(int line);


// reference to the TED chip
#define ted	(*((struct TED *)0xff00))

#pragma compile("ted.c")

#endif

