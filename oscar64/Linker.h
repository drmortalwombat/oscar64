#pragma once

#include "MachineTypes.h"
#include "Ident.h"
#include "Array.h"
#include "Errors.h"

struct LinkerReference
{
	int		mSection, mID, mOffset;
	int		mRefSection, mRefID, mRefOffset;
	bool	mLowByte, mHighByte;
};

struct LinkerSection
{
	const Ident* mIdent;
	int	mID;		
	int	mStart, mSize;
};

struct LinkerObject
{
	const Ident* mIdent;
	int	mID;
	int	mSize;
	uint8* mData;
};

class Linker
{
public:
	Linker(Errors * errors);
	~Linker(void);

	int AddSection(const Ident* section, int start, int size);
	void AddSectionData(const Ident* section, int id, const uint8* data, int size);
	uint8 * AddSectionSpace(const Ident* section, int id, int size);
	void AddReference(const LinkerReference& ref);
	
	GrowingArray<LinkerReference*>	mReferences;
	GrowingArray<LinkerSection*>	mSections;
	GrowingArray<LinkerObject*>		mObjects;

	void Link(void);
protected:
	Errors* mErrors;
};
