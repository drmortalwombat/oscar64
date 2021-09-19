#pragma once

#include "MachineTypes.h"
#include "Ident.h"
#include "Array.h"
#include "Errors.h"
#include "Disassembler.h"

class InterCodeProcedure;

enum LinkerObjectType
{
	LOT_NONE,
	LOT_PAD,
	LOT_BASIC,
	LOT_BYTE_CODE,
	LOT_NATIVE_CODE,
	LOT_RUNTIME,	
	LOT_DATA,
	LOT_BSS,
	LOT_STACK
};

struct LinkerReference
{
	int		mID, mOffset;
	int		mRefID, mRefOffset;
	bool	mLowByte, mHighByte;
};

struct LinkerSection
{
	const Ident* mIdent;
	int	mID;		
	int	mStart, mSize, mUsed;
};

struct LinkerObject
{
	Location	mLocation;
	const Ident* mIdent, * mSection;
	LinkerObjectType	mType;
	int	mID;
	int	mAddress;
	int	mSize;
	LinkerSection* mLinkerSection;
	uint8* mData;
	InterCodeProcedure* mProc;
};

class Linker
{
public:
	Linker(Errors * errors);
	~Linker(void);

	int AddSection(const Ident* section, int start, int size);

	int AddObject(const Location & location, const Ident* ident, const Ident* section, LinkerObjectType type);
	void AddObjectData(int id, const uint8* data, int size);
	uint8 * AddObjectSpace(int id, int size);
	void AttachObjectProcedure(int id, InterCodeProcedure* proc);

	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename);

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;

	uint8	mMemory[0x10000];

	void Link(void);
protected:
	NativeCodeDisassembler	mNativeDisassembler;
	ByteCodeDisassembler	mByteCodeDisassembler;

	int	mProgramStart, mProgramEnd;

	Errors* mErrors;
};
