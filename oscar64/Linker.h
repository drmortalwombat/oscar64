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

	int		mStart, mEnd, mUsed, mNonzero;

	GrowingArray<LinkerSection*>	mSections;

	LinkerRegion(void);
};

static const uint32	LREF_LOWBYTE	=	0x00000001;
static const uint32	LREF_HIGHBYTE	=	0x00000002;
static const uint32 LREF_TEMPORARY  =	0x00000004;

class LinkerReference
{
public:
	LinkerObject* mObject, * mRefObject;
	int		mOffset, mRefOffset;
	uint32	mFlags;
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

static const uint32 LOBJF_REFERENCED = 0x00000001;
static const uint32 LOBJF_PLACED	 = 0x00000002;
static const uint32 LOBJF_NO_FRAME	 = 0x00000004;
static const uint32 LOBJF_INLINE	 = 0x00000008;

class LinkerObject
{
public:
	Location			mLocation;
	const Ident		*	mIdent;
	LinkerObjectType	mType;
	int					mID;
	int					mAddress;
	int					mSize;
	LinkerSection	*	mSection;
	uint8			*	mData;
	InterCodeProcedure* mProc;
	uint32				mFlags;
	uint8				mTemporaries[16], mTempSizes[16];
	int					mNumTemporaries;

	LinkerObject(void);
	~LinkerObject(void);

	void AddData(const uint8* data, int size);
	uint8* AddSpace(int size);

	GrowingArray<LinkerReference*>	mReferences;

	void AddReference(const LinkerReference& ref);
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

	LinkerObject* FindObjectByAddr(int addr);

	bool IsSectionPlaced(LinkerSection* section);

	LinkerObject * AddObject(const Location & location, const Ident* ident, LinkerSection * section, LinkerObjectType type);

//	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename);
	bool WriteLblFile(const char* filename);

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerRegion*>		mRegions;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;

	uint8	mMemory[0x10000];
	int	mProgramStart, mProgramEnd;

	void ReferenceObject(LinkerObject* obj);

	void CollectReferences(void);
	void Link(void);
protected:
	NativeCodeDisassembler	mNativeDisassembler;
	ByteCodeDisassembler	mByteCodeDisassembler;

	Errors* mErrors;
};
