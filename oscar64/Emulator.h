#pragma once

#include "Assembler.h"
#include "MachineTypes.h"

class Emulator
{
public:
	Emulator(void);
	~Emulator(void);

	uint8		mMemory[0x10000];
	int			mCycles[0x10000];

	int		mIP;
	uint8	mRegA, mRegX, mRegY, mRegS, mRegP;

	int Emulate(int startIP);
	bool EmulateInstruction(AsmInsType type, AsmInsMode mode, int addr, int & cycles);
protected:
	void UpdateStatus(uint8 result);
	void UpdateStatusCarry(uint8 result, bool carry);
	void DumpCycles(void);
};