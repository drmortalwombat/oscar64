#include "asm6502.h"

inline byte asm_np(byte * ip, AsmIns ins)
{
	ip[0] = ins & 0xff;
	return 1;
}

inline byte asm_ac(byte * ip, AsmIns ins)
{
	ip[0] = (ins & 0xff) | 0x08;
	return 1;
}

inline byte asm_zp(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = (ins & 0xff) | 0x04;
	ip[1] = addr;
	return 2;
}

inline byte asm_rl(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = ins & 0xff;
	ip[1] = addr;
	return 2;
}

inline byte asm_im(byte * ip, AsmIns ins, byte value)
{
	ip[0] = (ins & 0xff) | ((ins & 0x01) << 3);
	ip[1] = value;
	return 2;
}

inline byte asm_zx(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = (ins & 0xff) | 0x05;
	ip[1] = addr;
	return 2;
}

inline byte asm_zy(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = (ins & 0xff) | 0x05;
	ip[1] = addr;
	return 2;
}

inline byte asm_ab(byte * ip, AsmIns ins, unsigned addr)
{
	ip[0] = (ins & 0xff) ^ 0x0c;
	ip[1] = addr & 0xff;
	ip[2] = addr >> 8;
	return 3;
}

inline byte asm_in(byte * ip, AsmIns ins, unsigned addr)
{
	ip[0] = (ins & 0xff) ^ 0x2c;
	ip[1] = addr & 0xff;
	ip[2] = addr >> 8;
	return 3;
}

inline byte asm_ax(byte * ip, AsmIns ins, unsigned addr)
{
	ip[0] = (ins & 0xff) | 0x1c;
	ip[1] = addr & 0xff;
	ip[2] = addr >> 8;
	return 3;
}

inline byte asm_ay(byte * ip, AsmIns ins, unsigned addr)
{
	ip[0] = (ins & 0xff) | 0x18;
	ip[1] = addr & 0xff;
	ip[2] = addr >> 8;
	return 3;
}

inline byte asm_ix(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = (ins & 0xff) | 0x00;
	ip[1] = addr;
	return 2;
}

inline byte asm_iy(byte * ip, AsmIns ins, byte addr)
{
	ip[0] = (ins & 0xff) | 0x10;
	ip[1] = addr;
	return 2;
}
