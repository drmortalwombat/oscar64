#pragma once

#include "MachineTypes.h"
#include "Ident.h"
#include "Array.h"
#include "Errors.h"
#include "Disassembler.h"
#include "DiskImage.h"
#include "CompilerTypes.h"

class InterCodeProcedure;
class InterVariable;
class NativeCodeProcedure;

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
	LOT_INLAY,
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

	uint32				mFlags;
	int					mStart, mEnd, mUsed, mNonzero, mReloc;
	uint64				mCartridgeBanks;
	LinkerObject	*	mInlayObject;

	GrowingArray<LinkerSection*>	mSections;

	LinkerRegion(void);

	struct FreeChunk
	{
		int	mStart, mEnd;
		LinkerObject* mLastObject;
	};

	GrowingArray<FreeChunk>		mFreeChunks;
	LinkerObject			*	mLastObject;
	
	bool AllocateAppend(Linker* linker, LinkerObject* obj);
	bool Allocate(Linker * linker, LinkerObject* obj, bool merge, bool retry);
	void PlaceStackSection(LinkerSection* stackSection, LinkerSection* section);
};

static const uint32	LREF_LOWBYTE		=	0x00000001;
static const uint32	LREF_HIGHBYTE		=	0x00000002;
static const uint32 LREF_TEMPORARY		=	0x00000004;
static const uint32 LREF_INBLOCK		=	0x00000008;
static const uint32	LREF_LOWBYTE_OFFSET	=	0x00000010;
static const uint32 LREF_BREAKPOINT		=	0x00000020;

class LinkerReference
{
public:
	LinkerObject* mObject, * mRefObject;
	int		mOffset, mRefOffset;
	uint32	mFlags;

	bool operator==(const LinkerReference& ref);
	bool operator!=(const LinkerReference& ref);

};

static const uint32 LSECF_PACKED = 0x00000001;
static const uint32 LSECF_PLACED = 0x00000002;

class LinkerSection
{
public:
	const Ident* mIdent;

	GrowingArray <LinkerObject*>	 mObjects;
	GrowingArray <LinkerSection*>	 mSections;

	int								mStart, mEnd, mSize;
	LinkerSectionType				mType;
	uint32							mFlags;

	LinkerSection(void);

	void RemObject(LinkerObject* obj);
	void AddObject(LinkerObject* obj);
};

static const uint32 LOBJF_REFERENCED		= 0x00000001;
static const uint32 LOBJF_PLACED			= 0x00000002;
static const uint32 LOBJF_NO_FRAME			= 0x00000004;
static const uint32 LOBJF_INLINE			= 0x00000008;
static const uint32 LOBJF_CONST				= 0x00000010;
static const uint32 LOBJF_RELEVANT			= 0x00000020;
static const uint32 LOBJF_STATIC_STACK		= 0x00000040;
static const uint32 LOBJF_NO_CROSS			= 0x00000080;
static const uint32 LOBJF_ZEROPAGE			= 0x00000100;
static const uint32 LOBJF_FORCE_ALIGN		= 0x00000200;
static const uint32 LOBJF_ZEROPAGESET		= 0x00000400;
static const uint32 LOBJF_NEVER_CROSS		= 0x00000800;

static const uint32 LOBJF_ARG_REG_A			= 0x00001000;
static const uint32 LOBJF_ARG_REG_X			= 0x00002000;
static const uint32 LOBJF_ARG_REG_Y			= 0x00004000;

static const uint32 LOBJF_RET_REG_A			= 0x00010000;
static const uint32 LOBJF_RET_REG_X			= 0x00020000;
static const uint32 LOBJF_RET_REG_Y			= 0x00040000;

static const uint32 LOBJF_PRESERVE_REG_A	= 0x00100000;
static const uint32 LOBJF_PRESERVE_REG_X	= 0x00200000;
static const uint32 LOBJF_PRESERVE_REG_Y	= 0x00400000;

static const uint32 LOBJF_LOCAL_VAR = 0x01000000;
static const uint32 LOBJF_LOCAL_USED = 0x02000000;

class LinkerObjectRange
{
public:
	const Ident	*	mIdent;
	int				mOffset, mSize;
};

struct CodeLocation
{
	Location	mLocation;
	int			mStart, mEnd;
	bool		mWeak;
};


class LinkerObject
{
public:
	Location							mLocation;
	const Ident						*	mIdent, * mFullIdent;
	LinkerObjectType					mType;
	int									mID, mMapID;
	int									mAddress, mRefAddress;
	int									mSize, mAlignment, mStripe, mStartUsed, mEndUsed;
	LinkerSection					*	mSection;
	LinkerRegion					*	mRegion;
	uint8							*	mData, * mMemory;
	InterCodeProcedure				*	mProc, * mOwnerProc;
	NativeCodeProcedure				*	mNativeProc;
	InterVariable					*	mVariable;
	uint32								mFlags;
	uint8								mTemporaries[16], mTempSizes[16];
	int									mNumTemporaries;
	ZeroPageSet							mZeroPageSet;
	LinkerSection					*	mStackSection;
	LinkerObject					*	mPrefix, * mSuffix;
	int									mSuffixReference;

	ExpandingArray<LinkerObjectRange>	mRanges;
	ExpandingArray<CodeLocation>		mCodeLocations;
	ExpandingArray<LinkerObjectRange>	mZeroPageRanges;

	LinkerObject(void);
	~LinkerObject(void);

	LinkerObject* CloneAssembler(Linker * linker) const;

	void AddData(const uint8* data, int size);
	void AddLocations(const ExpandingArray<CodeLocation>& locations);
	uint8* AddSpace(int size);
	void EnsureSpace(int offset, int size);

	GrowingArray<LinkerReference*>	mReferences;

	LinkerReference* FindReference(int64 offset);

	void AddReference(const LinkerReference& ref);

	void MoveToSection(LinkerSection* section);

	void MarkRelevant(void);

	bool IsSameConst(const LinkerObject* obj) const;

	bool IsBefore(const LinkerObject* obj) const;
	int FirstBank(void) const;
};

class LinkerOverlay
{
public:
	LinkerOverlay(void);
	~LinkerOverlay(void);

	Location						mLocation;
	const Ident					*	mIdent;
	int								mBank;
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

	LinkerRegion* FindRegionOfSection(LinkerSection* section);

	LinkerObject* FindObjectByAddr(int addr, InterCodeProcedure* proc = nullptr);
	LinkerObject* FindObjectByAddr(int bank, int addr, InterCodeProcedure * proc = nullptr);

	bool IsSectionPlaced(LinkerSection* section);

	LinkerObject * AddObject(const Location & location, const Ident* ident, LinkerSection * section, LinkerObjectType type, int alignment = 1);
	LinkerObject* FindSame(LinkerObject* obj);

	LinkerOverlay* AddOverlay(const Location& location, const Ident* ident, int bank);

	void SortObjects(void);

//	void AddReference(const LinkerReference& ref);

	bool WritePrgFile(DiskImage * image, const char* filename);
	bool WritePrgFile(const char* filename, const char * pathname);
	bool WriteXexFile(const char* filename);
	bool WriteMapFile(const char* filename);
	bool WriteAsmFile(const char* filename, const char * version);
	bool WriteLblFile(const char* filename);
	bool WriteCrtFile(const char* filename, uint16 id);
	bool WriteBinFile(const char* filename);
	bool WriteNesFile(const char* filename, TargetMachine machine);
	bool WriteMlbFile(const char* filename, TargetMachine machine);
	bool WriteDbjFile(FILE * file);

	int TranslateMlbAddress(int address, int bank, TargetMachine machine);

	uint64							mCompilerOptions;

	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerRegion*>		mRegions;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;
	GrowingArray<LinkerOverlay*>	mOverlays;
	GrowingArray<uint32>			mBreakpoints;

	uint8	mMemory[0x10000], mWorkspace[0x10000];
	uint8	mCartridge[64][0x10000];

	bool	mCartridgeBankUsed[64];

	int	mCartridgeBankStart[64], mCartridgeBankEnd[64];

	int	mProgramStart, mProgramEnd;

	void ReferenceObject(LinkerObject* obj);
	
	void CheckDirectJumps(void);
	void CollectReferences(void);
	void CombineSameConst(void);
	void InlineSimpleJumps(void);
	void PatchReferences(bool inlays);
	void CopyObjects(bool inlays);
	void PlaceObjects(bool retry);
	void Link(void);
	void CollectBreakpoints(void);
protected:
	NativeCodeDisassembler	mNativeDisassembler;
	ByteCodeDisassembler	mByteCodeDisassembler;

	bool Forwards(LinkerObject* pobj, LinkerObject* lobj);
	void SortObjectsPartition(int l, int r);

	Errors* mErrors;
};
