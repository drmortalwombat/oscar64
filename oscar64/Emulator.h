#pragma once

#include "Assembler.h"
#include "MachineTypes.h"

class Linker;

static const int TRACEF_BYTECODE = 0x01;
static const int TRACEF_NATIVE = 0x02;

class Emulator
{
public:
	Emulator(Linker * linker);
	~Emulator(void);

	uint8		mMemory[0x10000];
	int			mCycles[0x10000];
	bool		mCalls[0x100];

	int		mIP, mExitIP;
	uint8	mRegA, mRegX, mRegY, mRegS, mRegP;
	bool	mJiffies;

	Linker* mLinker;

	int Emulate(int startIP, int exitIP, int trace);
	void DumpProfile(void);
	bool EmulateInstruction(AsmInsType type, AsmInsMode mode, int addr, int & cycles, bool cross, bool indexed);
protected:
	void UpdateStatus(uint8 result);
	void UpdateStatusCarry(uint8 result, bool carry);
	void DumpCycles(void);
	void DumpCallstack(void);
};