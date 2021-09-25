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
	LOT_HEAP,
	LOT_STACK,
	LOT_SECTION_START,
	LOT_SECTION_END
};

enum LinkerSectionType
{
	LST_NONE,
	LST_DATA,
	LST_BSS,
	LST_HEAP,
	LST_STACK	
};

class LinkerObject;
class LinkerSection;

class LinkerRegion
{
public:
	const Ident* mIdent;

	int		mStart, mEnd, mUsed;

	GrowingArray<LinkerSection*>	mSections;

	LinkerRegion(void);
};

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

	GrowingArray <LinkerObject*>	 mObjects;


	int								mStart, mEnd, mSize;
	LinkerSectionType				mType;

	LinkerSection(void);
};

class LinkerObject
{
public:
	Location	mLocation;
	const Ident* mIdent;
	LinkerObjectType	mType;
	int	mID;
	int	mAddress;
	int	mSize;
	LinkerSection* mSection;
	uint8* mData;
	InterCodeProcedure* mProc;
	bool	mReferenced, mPlaced;

	void AddData(const uint8* data, int size);
	uint8* AddSpace(int size);
};

class Linker
{
public:
	Linker(Errors * errors);
	~Linker(void);

	LinkerRegion * AddRegion(const Ident* region, int start, int end);
	LinkerRegion* FindRegion(const Ident* region);

	LinkerSection * AddSection(const Ident* section, LinkerSectionType type);
	LinkerSection* FindSection(const Ident* section);

	bool IsSectionPlaced(LinkerSection* section);

	LinkerObject * AddObject(const Location & location, const Ident* ident, LinkerSection * section, LinkerObjectType type);

	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename);

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerRegion*>		mRegions;
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
