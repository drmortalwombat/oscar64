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

class LinkerObject;

class LinkerReference
{
public:
	LinkerObject* mObject, * mRefObject;
	int		mOffset, mRefOffset;
	bool	mLowByte, mHighByte;
};

class LinkerSection
{
public:
	const Ident* mIdent;
	int	mID;		
	int	mStart, mSize, mUsed;
};

class LinkerObject
{
public:
	Location	mLocation;
	const Ident* mIdent, * mSection;
	LinkerObjectType	mType;
	int	mID;
	int	mAddress;
	int	mSize;
	LinkerSection* mLinkerSection;
	uint8* mData;
	InterCodeProcedure* mProc;
	bool	mReferenced;

	void AddData(const uint8* data, int size);
	uint8* AddSpace(int size);
};

class Linker
{
public:
	Linker(Errors * errors);
	~Linker(void);

	int AddSection(const Ident* section, int start, int size);

	LinkerObject * AddObject(const Location & location, const Ident* ident, const Ident* section, LinkerObjectType type);

	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename);

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;

	uint8	mMemory[0x10000];
	int	mProgramStart, mProgramEnd;

	void ReferenceObject(LinkerObject* obj);

	void Link(void);
protected:
	NativeCodeDisassembler	mNativeDisassembler;
	ByteCodeDisassembler	mByteCodeDisassembler;

	Errors* mErrors;
};
