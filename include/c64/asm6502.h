#ifndef C64_ASM_6502_H
#define C64_ASM_6502_H

#include "types.h"

// Base form for the 6502 instructions

enum AsmIns
{
	// Implied
	
	ASM_BRK = 0x00,
	
	ASM_RTI = 0x40,
	ASM_RTS = 0x60,
	
	ASM_PHP = 0x08,
	ASM_CLC = 0x18,
	ASM_PLP = 0x28,
	ASM_SEC = 0x38,
	ASM_PHA = 0x48,
		
	ASM_CLI = 0x58,
	ASM_PLA = 0x68,
	ASM_SEI = 0x78,
	
	ASM_DEY = 0x88,
	ASM_TYA = 0x98,
	ASM_TAY = 0xa8,
	ASM_CLV = 0xb8,
	
	ASM_INY = 0xc8,
	ASM_CLD = 0xd8,
	ASM_INX = 0xe8,
	ASM_SED = 0xf8,
	
	ASM_TXA = 0x8a,
	ASM_TXS = 0x9a,
	ASM_TAX = 0xaa,
	ASM_TSX = 0xba,
	ASM_DEX = 0xca,
	ASM_NOP = 0xea,
	
	// Relative
	ASM_BPL = 0x10,
	ASM_BMI = 0x30,
	ASM_BVC = 0x50,
	ASM_BVS = 0x70,
	ASM_BCC = 0x90,
	ASM_BCS = 0xb0,
	ASM_BNE = 0xd0,
	ASM_BEQ = 0xf0,
	
	// Generic address	
	ASM_ORA = 0x01,
	ASM_AND = 0x21,
	ASM_EOR = 0x41,
	ASM_ADC = 0x61,
	ASM_STA = 0x81,
	ASM_LDA = 0xa1,
	ASM_CMP = 0xc1,
	ASM_SBC = 0xe1,
	
	ASM_STY = 0x80,
	ASM_LDY = 0xa0,
	ASM_CPY = 0xc0,
	ASM_CPX = 0xe0,
	
	ASM_ASL = 0x02,
	ASM_ROL = 0x22,
	ASM_LSR = 0x42,
	ASM_ROR = 0x62,
	
	ASM_STX = 0x82,	
	ASM_LDX = 0xa2,
	ASM_DEC = 0xc2,
	ASM_INC = 0xe2,
	
	// Limited Generic
	ASM_BIT = 0x20,
	
	// Jump
	ASM_JMP = 0x40,
	ASM_JSR = 0x2c
};

// the asm_ instructions emit a machine instruction at the given
// location and return the size.

// implied
inline byte asm_np(byte * ip, AsmIns ins);

// accu (e.g. rol/ror)
inline byte asm_ac(byte * ip, AsmIns ins);

// zero page
inline byte asm_zp(byte * ip, AsmIns ins, byte addr);

// relative branch
inline byte asm_rl(byte * ip, AsmIns ins, sbyte addr);

// immediate
inline byte asm_im(byte * ip, AsmIns ins, byte value);

// zero page indexed by x
inline byte asm_zx(byte * ip, AsmIns ins, byte addr);

// zero page indexed by y
inline byte asm_zy(byte * ip, AsmIns ins, byte addr);

// absolute
inline byte asm_ab(byte * ip, AsmIns ins, unsigned addr);

// indirect (jmp)
inline byte asm_in(byte * ip, AsmIns ins, unsigned addr);

// absolute indexed by x
inline byte asm_ax(byte * ip, AsmIns ins, unsigned addr);

// absolute indexed by y
inline byte asm_ay(byte * ip, AsmIns ins, unsigned addr);

// zero page indirect indexed by x
inline byte asm_ix(byte * ip, AsmIns ins, byte addr);

// zero page indirect indexed by y
inline byte asm_iy(byte * ip, AsmIns ins, byte addr);

#pragma compile("asm6502.c")

#endif

