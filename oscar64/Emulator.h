#pragma once

#include "Assembler.h"
#include "MachineTypes.h"

class Linker;

class Emulator
{
public:
	Emulator(Linker * linker);
	~Emulator(void);

	uint8		mMemory[0x10000];
	int			mCycles[0x10000];

	int		mIP;
	uint8	mRegA, mRegX, mRegY, mRegS, mRegP;
	bool	mJiffies;

	Linker* mLinker;

	int Emulate(int startIP, int trace);
	void DumpProfile(void);
	bool EmulateInstruction(AsmInsType type, AsmInsMode mode, int addr, int & cycles, bool cross, bool indexed);
protected:
	void UpdateStatus(uint8 result);
	void UpdateStatusCarry(uint8 result, bool carry);
	void DumpCycles(void);
};