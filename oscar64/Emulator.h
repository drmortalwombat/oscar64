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

	int		mIP, mExitIP;
	uint8	mRegA, mRegX, mRegY, mRegS, mRegP;
	bool	mJiffies, mUseIORange, mFileIO;
	int		mCycleCount;

	struct IORange
	{
		uint8	mCount0, mCount1, mMirror;
		uint8	mScratch[4];
		uint16	mDMAAddress;
		uint32	mCycleCount;
	}	mIORange;

	Linker* mLinker;

	bool		mCalls[0x100];
	uint8		mMemory[0x10000];
	int			mCycles[0x10000];

	int Emulate(int startIP, int exitIP, int trace, bool iorange, bool fileio);
	void DumpProfile(void);
	bool EmulateInstruction(AsmInsType type, AsmInsMode mode, int addr, int & cycles, bool cross, bool indexed);
protected:
	void UpdateStatus(uint8 result);
	void UpdateStatusCarry(uint8 result, bool carry);
	void DumpCycles(void);
	void DumpCallstack(void);

	uint8 ReadMemory(uint16 addr);
	void WriteMemory(uint16 addr, uint8 data);

	void EmulateKernal(void);

	// Kernal Variables

	uint16		mFileNamePos;
	uint8		mFileNameLength;
	uint8		mLFSNum, mLFSDevice, mLFSChannel;
	uint8		mInFile, mOutFile, mFileStatus;

	struct KernalFile
	{
		FILE	*	mFile;
		uint8		mNum, mDevice, mChannel;
		bool		mOpen;
	}	mFiles[16];
};