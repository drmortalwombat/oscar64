#ifndef C128_VDC_H
#define C128_VDC_H

#include <c64/types.h>

enum VDCRegister
{
	VDCR_HTOTAL,
	VDCR_HDISPLAY,
	VDCR_HSYNC,
	VDCR_SYNCSIZE,

	VDCR_VTOTAL,
	VDCR_VADJUST,
	VDCR_VDISPLAY,
	VDCR_VSYNC,

	VDCR_LACE,
	VDCR_CSIZE,
	VDCR_CURSOR_START,
	VDCR_CURSOR_END,

	VDCR_DISP_ADDRH,
	VDCR_DISP_ADDRL,
	VDCR_CURSOR_ADDRH,
	VDCR_CURSOR_ADDRL,

	VDCR_LPEN_Y,
	VDCR_LPEN_X,
	VDCR_ADDRH,
	VDCR_ADDRL,

	VDCR_ATTR_ADDRH,
	VDCR_ATTR_ADDRL,
	VDCR_CWIDTH,
	VDCR_CHEIGHT,

	VDCR_VSCROLL,
	VDCR_HSCROLL,
	VDCR_COLOR,
	VDCR_ROWINC,

	VDCR_CHAR_ADDRH,
	VDCR_UNDERLINE,
	VDCR_DSIZE,
	VDCR_DATA,

	VDCR_BLOCK_ADDRH,
	VDCR_BLOCK_ADDRL,
	VDCR_HSTART,
	VDCR_HEND,

	VDCR_REFRESH
};

struct VDC
{
	volatile char addr;
	volatile char data;
};

#define vdc	(*((struct VDC *)0xd600))

inline void vdc_reg(VDCRegister reg);

inline void vdc_write(byte data);

inline byte vdc_read(void);


void vdc_reg_write(VDCRegister reg, byte data);

byte vdc_reg_read(VDCRegister reg);


void vdc_mem_addr(unsigned addr);

inline void vdc_mem_write(char data);

inline char vdc_mem_read(void);


void vdc_mem_write_at(unsigned addr, char data);

char vdc_mem_read_at(unsigned addr);


void vdc_mem_write_buffer(unsigned addr, const char * data, char size);

void vdc_mem_read_buffer(unsigned addr, char * data, char size);


#pragma compile("vdc.c")

#endif
