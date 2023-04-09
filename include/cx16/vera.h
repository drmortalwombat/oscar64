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

#define VERA_DCVIDEO_MODE_OFF		0x00
#define VERA_DCVIDEO_MODE_VGA		0x01
#define VERA_DCVIDEO_MODE_NTSC		0x02
#define VERA_DCVIDEO_MODE_RGBI		0x03

#define VERA_DCVIDEO_NCHROMA		0x04
#define VERA_DCVIDEO_LAYER0			0x10
#define VERA_DCVIDEO_LAYER1			0x20
#define VERA_DCVIDEO_SPRITES		0x40

#define VERA_LAYER_DEPTH_1		0x00
#define VERA_LAYER_DEPTH_2		0x01
#define VERA_LAYER_DEPTH_4		0x02
#define VERA_LAYER_DEPTH_8		0x03

#define VERA_LAYER_BITMAP		0x04
#define VERA_LAYER_T256C		0x08
#define VERA_LAYER_WIDTH_32		0x00
#define VERA_LAYER_WIDTH_64		0x10
#define VERA_LAYER_WIDTH_128	0x20
#define VERA_LAYER_WIDTH_256	0x30
#define VERA_LAYER_HEIGHT_32	0x00
#define VERA_LAYER_HEIGHT_64	0x40
#define VERA_LAYER_HEIGHT_128	0x80
#define VERA_LAYER_HEIGHT_256	0xc0

#define VERA_TILE_WIDTH_8		0x00
#define VERA_TILE_WIDTH_16		0x01
#define VERA_TILE_HEIGHT_8		0x00
#define VERA_TILE_HEIGHT_16		0x02


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

enum VERASpriteMode
{
	VSPRMODE_4,
	VSPRMODE_8
};

enum VERASpriteSize
{
	VSPRSZIZE_8,
	VSPRSZIZE_16,
	VSPRSZIZE_32,
	VSPRSZIZE_64
};

enum VERASpritePriority
{
	VSPRPRI_OFF,
	VSPRPRI_BACK,
	VSPRPRI_MIDDLE,
	VSPRPRI_FRONT
};

#define VERA_COLOR(r, g, b) (((unsigned)(r) << 8) | ((unsigned)(g) << 4) | (unsigned)(b))

#define vera    (*(VERA *)0x9f20)

inline void vram_addr(unsigned long addr);

inline void vram_put(char data);

inline void vram_putw(unsigned data);

inline char vram_get(void);

inline unsigned vram_getw(void);

inline void vram_put_at(unsigned long addr, char data);

inline char vram_get_at(unsigned long addr);

void vram_putn(unsigned long addr, const char * data, unsigned size);

void vram_getn(unsigned long addr, char * data, unsigned size);

void vram_fill(unsigned long addr, char data, unsigned size);

void vera_spr_set(char spr, unsigned addr32, VERASpriteMode mode8, VERASpriteSize w, VERASpriteSize h, VERASpritePriority z, char pal);

void vera_spr_move(char spr, int x, int y);

void vera_spr_image(char spr, unsigned addr32);

void vera_pal_put(char index, unsigned color);

unsigned vera_pal_get(char index);

void vera_pal_putn(char index, const unsigned * color, unsigned size);

#pragma compile("vera.c")

#endif
