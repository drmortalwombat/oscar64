#ifndef CX16_VERA_H
#define CX16_VERA_H

// Thanks to crisps for providing the initial code for this library

#include <c64/types.h>

#define VERA_ADDRH_DECR		0x08
#define VERA_ADDRH_INC		0xf0

#define VERA_CTRL_RESET		0x80
#define VERA_CTRL_DCSEL		0x02
#define VERA_CTRL_ADDRSEL	0x01

#define VERA_IRQ_LINE_8		0x80
#define VERA_IRQ_AFLOW		0x08
#define VERA_IRQ_SPRCOL		0x04
#define VERA_IRQ_LINE		0x02
#define VERA_IRQ_VSYNC		0x01


struct VERA
{
	volatile word	addr;
	volatile byte	addrh;
	volatile byte	data0, data1;

	volatile byte	ctrl;
	volatile byte	ien;
	volatile byte	isr;
	volatile byte	irqline;

	volatile byte	dcvideo;
	volatile byte	dchscale;
	volatile byte	dcvscale;
	volatile byte	dcborder;

	volatile byte	l0config;
	volatile byte	l0mapbase;
	volatile byte	l0tilebase;
	volatile word	l0hscroll;
	volatile word	l0vscroll;

	volatile byte	l1config;
	volatile byte	l1mapbase;
	volatile byte	l1tilebase;
	volatile word	l1hscroll;
	volatile word	l1vscroll;

	volatile byte	audioctrl;
	volatile byte	audiorate;
	volatile byte	audiodata;

	volatile byte	spidata;
	volatile byte	spictrl;
};

#define vera    (*(VERA *)0x9f20)

inline void vram_addr(unsigned long addr);

inline void vram_put(char data);

inline void vram_putw(unsigned data);

inline char vram_get(void);

inline void vram_put_at(unsigned long addr, char data);

inline char vram_get_at(unsigned long addr);

void vram_putn(unsigned long addr, const char * data, unsigned size);

void vram_getn(unsigned long addr, char * data, unsigned size);

void vram_fill(unsigned long addr, char data, unsigned size);

void vera_spr_set(char spr, unsigned addr32, bool mode8, char w, char h, char z, char pal);

void vera_spr_move(char spr, int x, int y);

#pragma compile("vera.c")

#endif
