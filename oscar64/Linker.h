#pragma once

#include "MachineTypes.h"
#include "Ident.h"
#include "Array.h"
#include "Errors.h"
#include "Disassembler.h"
#include "DiskImage.h"

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
	LST_STACK,
	LST_STATIC_STACK,
	LST_ZEROPAGE

};

struct ZeroPageSet
{
	ZeroPageSet(void)
	{
		for (int i = 0; i < 8; i++)
			mBits[i] = 0;
	}

	uint32		mBits[8];

	void operator += (int n)
	{
		mBits[n >> 5] |= 1 << (n & 31);
	}

	void operator -= (int n)
	{
		mBits[n >> 5] &= ~(1 << (n & 31));
	}

	bool operator[] (int n) const
	{
		return (mBits[n >> 5] & (1 << (n & 31))) != 0;
	}

	ZeroPageSet& operator |= (const ZeroPageSet& set)
	{
		for (int i = 0; i < 8; i++)
			mBits[i] |= set.mBits[i];
		return *this;
	}
};

class LinkerObject;
class LinkerSection;

class LinkerRegion
{
public:
	const Ident* mIdent;

	uint32	mFlags;
	int		mStart, mEnd, mUsed, mNonzero, mReloc;
	uint64	mCartridgeBanks;

	GrowingArray<LinkerSection*>	mSections;

	LinkerRegion(void);

	struct FreeChunk
	{
		int	mStart, mEnd;
	};

	GrowingArray<FreeChunk>		mFreeChunks;
	
	bool Allocate(Linker * linker, LinkerObject* obj);
	void PlaceStackSection(LinkerSection* stackSection, LinkerSection* section);
};

static const uint32	LREF_LOWBYTE	=	0x00000001;
static const uint32	LREF_HIGHBYTE	=	0x00000002;
static const uint32 LREF_TEMPORARY  =	0x00000004;
static const uint32 LREF_INBLOCK	=	0x00000008;

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
	GrowingArray <LinkerSection*>	 mSections;

	int								mStart, mEnd, mSize;
	LinkerSectionType				mType;

	LinkerSection(void);

	void RemObject(LinkerObject* obj);
	void AddObject(LinkerObject* obj);
};

static const uint32 LOBJF_REFERENCED	= 0x00000001;
static const uint32 LOBJF_PLACED		= 0x00000002;
static const uint32 LOBJF_NO_FRAME		= 0x00000004;
static const uint32 LOBJF_INLINE		= 0x00000008;
static const uint32 LOBJF_CONST			= 0x00000010;
static const uint32 LOBJF_RELEVANT		= 0x00000020;
static const uint32 LOBJF_STATIC_STACK	= 0x00000040;
static const uint32 LOBJF_NO_CROSS		= 0x00000080;
static const uint32 LOBJF_ZEROPAGE		= 0x00000100;

class LinkerObject
{
public:
	Location						mLocation;
	const Ident					*	mIdent;
	LinkerObjectType				mType;
	int								mID;
	int								mAddress, mRefAddress;
	int								mSize, mAlignment;
	LinkerSection				*	mSection;
	LinkerRegion				*	mRegion;
	uint8						*	mData;
	InterCodeProcedure			*	mProc;
	uint32							mFlags;
	uint8							mTemporaries[16], mTempSizes[16];
	int								mNumTemporaries;
	ZeroPageSet						mZeroPageSet;
	LinkerSection				*	mStackSection;

	LinkerObject(void);
	~LinkerObject(void);

	void AddData(const uint8* data, int size);
	uint8* AddSpace(int size);

	GrowingArray<LinkerReference*>	mReferences;

	void AddReference(const LinkerReference& ref);

	void MoveToSection(LinkerSection* section);
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

	LinkerObject * AddObject(const Location & location, const Ident* ident, LinkerSection * section, LinkerObjectType type, int alignment = 1);

//	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(DiskImage * image);
	bool WritePrgFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename);
	bool WriteLblFile(const char* filename);
	bool WriteCrtFile(const char* filename);
	bool WriteBinFile(const char* filename);

	uint64							mCompilerOptions;

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerRegion*>		mRegions;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;

	uint8	mMemory[0x10000];
	uint8	mCartridge[64][0x4000];

	bool	mCartridgeBankUsed[64];

	int	mProgramStart, mProgramEnd;

	void ReferenceObject(LinkerObject* obj);

	void CollectReferences(void);
	void Link(void);
protected:
	NativeCodeDisassembler	mNativeDisassembler;
	ByteCodeDisassembler	mByteCodeDisassembler;

	Errors* mErrors;
};
