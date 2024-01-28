#include "vdc.h"


inline void vdc_reg(VDCRegister reg)
{
	vdc.addr = reg;
}

inline void vdc_write(byte data)
{
	do {} while (vdc.addr < 128);
	vdc.data = data;
}

inline byte vdc_read(void)
{
	do {} while (vdc.addr < 128);
	return vdc.data;
}


void vdc_reg_write(VDCRegister reg, byte data)
{
	vdc_reg(reg);
	vdc_write(data);
}

byte vdc_reg_read(VDCRegister reg)
{
	vdc_reg(reg);
	return vdc_read();
}


void vdc_mem_addr(unsigned addr)
{
	vdc_reg_write(VDCR_ADDRL, addr);
	vdc_reg_write(VDCR_ADDRH, addr >> 8);
	vdc_reg(VDCR_DATA);
}

inline void vdc_mem_write(char data)
{
	vdc_write(data);
}

inline char vdc_mem_read(void)
{
	return vdc_read();
}


void vdc_mem_write_at(unsigned addr, char data)
{
	vdc_mem_addr(addr);
	vdc_write(data);
}

char vdc_mem_read_at(unsigned addr)
{
	vdc_mem_addr(addr);
	return vdc_read();
}


void vdc_mem_write_buffer(unsigned addr, const char * data, char size)
{
	vdc_mem_addr(addr);
	for(char i=0; i<size; i++)
		vdc_write(data[i]);
}

void vdc_mem_read_buffer(unsigned addr, char * data, char size)
{
	vdc_mem_addr(addr);
	for(char i=0; i<size; i++)
		data[i] = vdc_read();
}

