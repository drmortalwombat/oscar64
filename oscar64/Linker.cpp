#include "Linker.h"
#include <string.h>
#include <stdio.h>
#include "CompilerTypes.h"
#include "Compression.h"

LinkerRegion::LinkerRegion(void)
	: mSections(nullptr), mFreeChunks(FreeChunk{ 0, 0 } ), mLastObject(nullptr), mInlayObject(nullptr), mCartridgeBanks(0)
{}

LinkerSection::LinkerSection(void)
	: mObjects(nullptr), mSections(nullptr), mFlags(0)
{}


void LinkerSection::RemObject(LinkerObject* obj)
{
	int i = mObjects.IndexOf(obj);
	if (i >= 0)
	{
		mObjects.Remove(i);
		obj->mSection = nullptr;
	}
}

void LinkerSection::AddObject(LinkerObject* obj)
{
	mObjects.Push(obj);
	obj->mSection = this;
}

bool LinkerReference::operator==(const LinkerReference& ref)
{
	return
		mFlags == ref.mFlags &&
		mOffset == ref.mOffset &&
		mRefOffset == ref.mRefOffset &&
//		mObject->mMapID == ref.mObject->mMapID &&
		mRefObject->mMapID == ref.mRefObject->mMapID;
}

bool LinkerReference::operator!=(const LinkerReference& ref)
{
	return !(*this == ref);
}

LinkerObject::LinkerObject(void)
	: mReferences(nullptr), mNumTemporaries(0), mSize(0), mAlignment(1), mStackSection(nullptr), mIdent(nullptr), mFullIdent(nullptr), mStartUsed(0x10000), mEndUsed(0x00000), mMemory(nullptr)
	, mPrefix(nullptr), mSuffix(nullptr), mProc(nullptr), mNativeProc(nullptr)
{}

LinkerObject::~LinkerObject(void)
{

}

void LinkerObject::AddReference(const LinkerReference& ref)
{
	LinkerReference* nref = new LinkerReference(ref);
	mReferences.Push(nref);
}

LinkerReference* LinkerObject::FindReference(int64 offset)
{
	for (int i = 0; i < mReferences.Size(); i++)
	{
		if (mReferences[i]->mOffset == offset)
			return mReferences[i];
		else if ((mReferences[i]->mFlags & LREF_LOWBYTE) && (mReferences[i]->mFlags & LREF_HIGHBYTE) && mReferences[i]->mOffset + 1 == offset)
			return mReferences[i];
	}

	return nullptr;
}


void LinkerObject::MarkRelevant(void)
{
	if (!(mFlags & LOBJF_RELEVANT))
	{
		mFlags |= LOBJF_RELEVANT;
		for (int i = 0; i < mReferences.Size(); i++)
			if (mReferences[i]->mRefObject)
				mReferences[i]->mRefObject->MarkRelevant();
	}
}

void LinkerObject::MoveToSection(LinkerSection* section)
{
	if (section != mSection)
	{
		if (mSection)
			mSection->RemObject(this);
		section->AddObject(this);
	}
}

void LinkerObject::AddData(const uint8* data, int size)
{
	mSize = size;
	mData = new uint8[size];
	memcpy(mData, data, size);
}

void LinkerObject::AddLocations(const ExpandingArray<CodeLocation>& locations)
{
	for (int i = 0; i < locations.Size(); i++)
		mCodeLocations.Push(locations[i]);
}


void LinkerObject::EnsureSpace(int offset, int size)
{
	if (offset + size > mSize)
		AddSpace(offset + size);
}

uint8* LinkerObject::AddSpace(int size)
{
	if (mSize != size)
	{
		mSize = size;
		mData = new uint8[size];
		memset(mData, 0, size);
	}
	return mData;
}

LinkerOverlay::LinkerOverlay(void)
{

}

LinkerOverlay::~LinkerOverlay(void)
{

}

Linker::Linker(Errors* errors)
	: mErrors(errors), mSections(nullptr), mReferences(nullptr), mObjects(nullptr), mRegions(nullptr), mOverlays(nullptr), mBreakpoints(0), mCompilerOptions(COPT_DEFAULT)
{
	for (int i = 0; i < 64; i++)
	{
		mCartridgeBankUsed[i] = 0;
		mCartridgeBankStart[i] = 0x10000;
		mCartridgeBankEnd[i] = 0x00000;
		memset(mCartridge[i], 0, 0x10000);
	}
	memset(mMemory, 0, 0x10000);
}

Linker::~Linker(void)
{

}


LinkerRegion* Linker::AddRegion(const Ident* region, int start, int end)
{
	LinkerRegion* lrgn = new LinkerRegion();
	lrgn->mIdent = region;
	lrgn->mStart = start;
	lrgn->mReloc = 0;
	lrgn->mEnd = end;
	lrgn->mUsed = 0;
	lrgn->mNonzero = 0;
	lrgn->mCartridgeBanks = 0;
	lrgn->mFlags = 0;
	mRegions.Push(lrgn);
	return lrgn;
}

LinkerRegion* Linker::FindRegion(const Ident* region)
{
	for (int i = 0; i < mRegions.Size(); i++)
	{
		if (mRegions[i]->mIdent == region)
			return mRegions[i];
	}

	return nullptr;
}

LinkerSection* Linker::AddSection(const Ident* section, LinkerSectionType type)
{
	LinkerSection* lsec = new LinkerSection;
	lsec->mIdent = section;
	lsec->mType = type;
	mSections.Push(lsec);
	return lsec;

}

LinkerRegion* Linker::FindRegionOfSection(LinkerSection* section)
{
	LinkerRegion* srgn = nullptr;
	for (int i = 0; i < mRegions.Size(); i++)
	{
		LinkerRegion* rgn = mRegions[i];
		if (rgn->mSections.Contains(section))
		{
			if (srgn)
				return nullptr;
			srgn = rgn;
		}
	}

	return srgn;
}

LinkerSection* Linker::FindSection(const Ident* section)
{
	for (int i = 0; i < mSections.Size(); i++)
	{
		if (mSections[i]->mIdent == section)
			return mSections[i];
	}

	return nullptr;
}

LinkerOverlay* Linker::AddOverlay(const Location& location, const Ident* ident, int bank)
{
	LinkerOverlay* lovl = new LinkerOverlay;
	lovl->mLocation = location;
	lovl->mIdent = ident;
	lovl->mBank = bank;
	mOverlays.Push(lovl);

	return lovl;
}

bool Linker::IsSectionPlaced(LinkerSection* section)
{
	for (int i = 0; i < mRegions.Size(); i++)
	{
		LinkerRegion* rgn = mRegions[i];
		for (int j = 0; j < rgn->mSections.Size(); j++)
			if (section == rgn->mSections[j])
				return true;
	}

	return false;
}

LinkerObject* Linker::FindObjectByAddr(int addr)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* lobj = mObjects[i];
		if (lobj->mFlags & LOBJF_PLACED)
		{
			if (addr >= lobj->mAddress && addr < lobj->mAddress + lobj->mSize)
				return lobj;
		}
	}

	return nullptr;
}

LinkerObject* Linker::FindObjectByAddr(int bank, int addr)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* lobj = mObjects[i];
		if (lobj->mFlags & LOBJF_PLACED)
		{
			if (lobj->mRegion && ((1ULL << bank) & lobj->mRegion->mCartridgeBanks))
			{
				if (addr >= lobj->mAddress && addr < lobj->mAddress + lobj->mSize)
					return lobj;
			}
		}
	}

	return FindObjectByAddr(addr);
}

LinkerObject* Linker::FindSame(LinkerObject* obj)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* lobj = mObjects[i];
		if (lobj != obj && obj->IsSameConst(lobj))
			return lobj;
	}

	return nullptr;
}

bool LinkerObject::IsSameConst(const LinkerObject* obj) const
{
	if ((mFlags & LOBJF_CONST) && mFlags == obj->mFlags && 
		mSection == obj->mSection && mSize == obj->mSize && mAlignment == obj->mAlignment &&
		mReferences.Size() == obj->mReferences.Size())
	{
		for (int i = 0; i < mSize; i++)
			if (mData[i] != obj->mData[i])
				return false;

		for (int i = 0; i < mReferences.Size(); i++)
			if (mReferences[i] != obj->mReferences[i])
				return false;

		return true;
	}

	return false;
}

int LinkerObject::FirstBank(void) const
{
	if (mRegion->mCartridgeBanks)
	{
		uint64	m = mRegion->mCartridgeBanks;
		int i = 1;
		while (!(m & 1))
		{
			i++;
			m >>= 1;
		}
		return i;
	}
	else
		return 0;
}

bool LinkerObject::IsBefore(const LinkerObject* obj) const
{
	if (mFlags & LOBJF_PLACED)
	{
		if (obj->mFlags & LOBJF_PLACED)
		{
			int	b0 = FirstBank(), b1 = obj->FirstBank();
			if (b0 < b1)
				return true;
			else if (b0 == b1)
				return mAddress < obj->mAddress;
			else
				return false;
		}
		else
			return true;
	}
	else
		return false;
}

LinkerObject * Linker::AddObject(const Location& location, const Ident* ident, LinkerSection * section, LinkerObjectType type, int alignment)
{
	LinkerObject* obj = new LinkerObject;
	obj->mLocation = location;
	obj->mID = obj->mMapID = mObjects.Size();
	obj->mType = type;
	obj->mData = nullptr;
	obj->mSize = 0;
	obj->mIdent = ident;
	obj->mFullIdent = ident;
	obj->mSection = section;
	obj->mRegion = nullptr;
	obj->mProc = nullptr;
	obj->mVariable = nullptr;
	obj->mFlags = 0;
	obj->mAlignment = alignment;
	section->mObjects.Push(obj);
	mObjects.Push(obj);
	return obj;
}

bool Linker::Forwards(LinkerObject* pobj, LinkerObject* lobj)
{
	if (lobj->mAlignment == 1 && pobj && lobj->mType == LOT_NATIVE_CODE && pobj->mType == LOT_NATIVE_CODE && lobj->mSection == pobj->mSection)
	{
		if (pobj->mSize >= 3 && pobj->mData[pobj->mSize - 3] == 0x4c && pobj->mReferences.Size() > 0)
		{
			int i = 0;
			while (i < pobj->mReferences.Size() && pobj->mReferences[i]->mOffset != pobj->mSize - 2)
				i++;
			if (i < pobj->mReferences.Size() && pobj->mReferences[i]->mRefObject == lobj && pobj->mReferences[i]->mRefOffset == 0)
			{
				if (mCompilerOptions & COPT_VERBOSE2)
					printf("Direct %s -> %s\n", pobj->mIdent->mString, lobj->mIdent->mString);

				pobj->mSuffixReference = i;

				return true;
			}
		}
	}

	return false;
}

void Linker::InlineSimpleJumps(void)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* cobj(mObjects[i]);
		if (cobj->mType == LOT_NATIVE_CODE)
		{
			for (int j = 0; j < cobj->mReferences.Size(); j++)
			{
				LinkerReference* cref = cobj->mReferences[j];
				if (cref->mOffset > 0 && cref->mOffset < cobj->mSize + 2 && 
					cobj->mData[cref->mOffset - 1] == 0x20 &&
					(cref->mFlags & (LREF_HIGHBYTE | LREF_LOWBYTE)) == (LREF_HIGHBYTE | LREF_LOWBYTE))
				{
					if (cref->mRefObject->mType == LOT_NATIVE_CODE && cref->mRefObject->mSize == 3 && 
						cobj->mSection == cref->mRefObject->mSection && cref->mRefOffset == 0 &&
						cref->mRefObject->mData[0] == 0x4c)
					{
						LinkerObject* tobj(cref->mRefObject);
						if (tobj->mReferences.Size())
						{
							cref->mRefObject = tobj->mReferences[0]->mRefObject;
							cref->mRefOffset = tobj->mReferences[0]->mRefOffset;
						}
						else
						{
							cobj->mData[cref->mOffset + 0] = tobj->mData[1];
							cobj->mData[cref->mOffset + 1] = tobj->mData[2];
							cref->mFlags = 0;
							cref->mRefObject = cobj;
						}
					}
				}
			}
		}

	}

}


void Linker::CheckDirectJumps(void)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		for (int j = 0; j < mObjects.Size(); j++)
		{
			if (i != j && !mObjects[j]->mPrefix && Forwards(mObjects[i], mObjects[j]))
			{
				mObjects[i]->mSuffix = mObjects[j];
				mObjects[j]->mPrefix = mObjects[i];
				break;
			}
		}
	}
}

void Linker::CombineSameConst(void)
{
	bool changed = true;
	while (changed)
	{
		changed = false;

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* dobj(mObjects[i]);
			while (dobj->mMapID != mObjects[dobj->mMapID]->mMapID)
				dobj->mMapID = mObjects[dobj->mMapID]->mMapID;

			if ((dobj->mFlags & LOBJF_REFERENCED) && (dobj->mFlags & LOBJF_CONST) && dobj->mMapID == dobj->mID && dobj->mType != LOT_INLAY)
			{
				for (int j = i + 1; j < mObjects.Size(); j++)
				{
					LinkerObject* sobj(mObjects[j]);

					if ((sobj->mFlags & LOBJF_REFERENCED) && (sobj->mFlags & LOBJF_CONST) && sobj->mMapID == sobj->mID && sobj->mType != LOT_INLAY)
					{
						if (dobj->mSize == sobj->mSize && dobj->mSection == sobj->mSection && dobj->mReferences.Size() == sobj->mReferences.Size())
						{
							int i = 0;
							while (i < sobj->mSize && sobj->mData[i] == dobj->mData[i])
								i++;
							if (i == sobj->mSize)
							{
								i = 0;
								while (i < sobj->mReferences.Size() && *sobj->mReferences[i] == *dobj->mReferences[i])
									i++;
								if (i == sobj->mReferences.Size())
								{
									sobj->mMapID = dobj->mMapID;
									changed = true;

									if (dobj->mIdent && sobj->mIdent && (mCompilerOptions & COPT_VERBOSE2))
									{
										printf("Match %s : %s\n", dobj->mIdent->mString, sobj->mIdent->mString);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* lobj(mObjects[i]);
		if (lobj->mFlags & LOBJF_REFERENCED)
		{
			if (lobj->mMapID != lobj->mID)
				lobj->mFlags &= ~LOBJF_REFERENCED;
			else
			{
				for (int j = 0; j < lobj->mReferences.Size(); j++)
					lobj->mReferences[j]->mRefObject = mObjects[lobj->mReferences[j]->mRefObject->mMapID];
			}
		}
	}
}

void Linker::CollectReferences(void) 
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* lobj(mObjects[i]);
		for (int j = 0; j < lobj->mReferences.Size(); j++)
			mReferences.Push(lobj->mReferences[j]);
	}
}

void Linker::ReferenceObject(LinkerObject* obj)
{
	if (!(obj->mFlags & LOBJF_REFERENCED))
	{
		obj->mFlags |= LOBJF_REFERENCED;
		for (int i = 0; i < mReferences.Size(); i++)
		{
			LinkerReference* ref = mReferences[i];
			if (ref->mObject == obj)
				ReferenceObject(ref->mRefObject);
		}
	}
}

bool LinkerRegion::AllocateAppend(Linker* linker, LinkerObject* lobj)
{
	if (lobj->mPrefix && (lobj->mPrefix->mFlags & LOBJF_PLACED))
	{
		if (lobj->mPrefix == mLastObject)
		{
			int start = mStart + mUsed - 3;
			int end = start + lobj->mSize;

			if (end <= mEnd)
			{
				lobj->mPrefix->mReferences[lobj->mPrefix->mSuffixReference]->mFlags = 0;
				lobj->mPrefix->mSize -= 3;

				lobj->mFlags |= LOBJF_PLACED;
				lobj->mAddress = start;
				lobj->mRefAddress = start + mReloc;
				lobj->mRegion = this;
				mUsed = end - mStart;

				mLastObject = lobj;

				if (lobj->mSuffix && !(lobj->mSuffix->mFlags & LOBJF_PLACED))
				{
					if (!Allocate(linker, lobj->mSuffix, true, false))
						return false;
				}

				return true;
			}
		}
		else
		{
			int i = 0;
			while (i < mFreeChunks.Size() && lobj->mPrefix != mFreeChunks[i].mLastObject)
				i++;
			if (i < mFreeChunks.Size())
			{
				int start = mFreeChunks[i].mStart - 3;
				int end = start + lobj->mSize;

				if (end <= mFreeChunks[i].mEnd)
				{
					lobj->mPrefix->mReferences[lobj->mPrefix->mSuffixReference]->mFlags = 0;
					lobj->mPrefix->mSize -= 3;

					lobj->mFlags |= LOBJF_PLACED;
					lobj->mAddress = start;
					lobj->mRefAddress = start + mReloc;
					lobj->mRegion = this;

					if (end == mFreeChunks[i].mEnd)
						mFreeChunks.Remove(i);
					else
					{
						mFreeChunks[i].mStart = end;
						mFreeChunks[i].mLastObject = lobj;
					}

					if (lobj->mSuffix && !(lobj->mSuffix->mFlags & LOBJF_PLACED))
					{
						if (!Allocate(linker, lobj->mSuffix, true, false))
							return false;
					}

					return true;
				}
			}
		}
	}
	return false;
}

bool LinkerRegion::Allocate(Linker * linker, LinkerObject* lobj, bool merge, bool retry)
{
	if (merge && lobj->mPrefix)
	{
		if (!(lobj->mPrefix->mFlags & LOBJF_PLACED))
		{
			if (!Allocate(linker, lobj->mPrefix, true, false))
				return false;

			if (lobj->mFlags & LOBJF_PLACED)
				return true;
		}

		if (AllocateAppend(linker, lobj))
			return true;
	}	
		
	int i = 0;
	while (i < mFreeChunks.Size())
	{
		int start = (mFreeChunks[i].mStart + lobj->mAlignment - 1) & ~(lobj->mAlignment - 1);
		int end = start + lobj->mSize;

		if (!(linker->mCompilerOptions & COPT_OPTIMIZE_CODE_SIZE) && (lobj->mFlags & LOBJF_NO_CROSS) && lobj->mSize <= 256 && (start & 0xff00) != ((end - 1) & 0xff00) && !(lobj->mSection->mFlags & LSECF_PACKED))
			;
		else if (end <= mFreeChunks[i].mEnd)
		{
			if (merge && lobj->mPrefix && lobj->mPrefix == mFreeChunks[i].mLastObject)
			{
				lobj->mPrefix->mReferences[lobj->mPrefix->mSuffixReference]->mFlags = 0;
				lobj->mPrefix->mSize -= 3;

				start -= 3;
				end -= 3;
			}

			lobj->mFlags |= LOBJF_PLACED;
			lobj->mAddress = start;
			lobj->mRefAddress = start + mReloc;
			lobj->mRegion = this;

			if (start == mFreeChunks[i].mStart)
			{
				if (end == mFreeChunks[i].mEnd)
					mFreeChunks.Remove(i);
				else
				{
					mFreeChunks[i].mStart = end;
					mFreeChunks[i].mLastObject = lobj;
				}
			}
			else if (end == mFreeChunks[i].mEnd)
			{
				mFreeChunks[i].mEnd = start;
			}
			else
			{
				mFreeChunks.Insert(i + 1, FreeChunk{ end, mFreeChunks[i].mEnd, lobj });
				mFreeChunks[i].mEnd = start;
			}

			if (merge && lobj->mSuffix && !(lobj->mSuffix->mFlags & LOBJF_PLACED))
			{
				if (!Allocate(linker, lobj->mSuffix, true, false))
					return false;
			}

			return true;
		}
		i++;
	}

	int start = (mStart + mUsed + lobj->mAlignment - 1) & ~(lobj->mAlignment - 1);
	int end = start + lobj->mSize;

	if (!(linker->mCompilerOptions & COPT_OPTIMIZE_CODE_SIZE) && !retry && (lobj->mFlags & LOBJF_NO_CROSS) && !(lobj->mFlags & LOBJF_FORCE_ALIGN) && lobj->mSize <= 256 && (start & 0xff00) != ((end - 1) & 0xff00) && !(lobj->mSection->mFlags & LSECF_PACKED))
	{
		start = (start + 0x00ff) & 0xff00;
		end = start + lobj->mSize;
	}

	if (end <= mEnd)
	{
		// Check if directly follows an object that jumps to this new object
		if (merge && lobj->mPrefix && lobj->mPrefix == mLastObject)
		{
			lobj->mPrefix->mReferences[lobj->mPrefix->mSuffixReference]->mFlags = 0;
			lobj->mPrefix->mSize -= 3;

			start -= 3;
			end -= 3;
			mLastObject = nullptr;
		}

		lobj->mFlags |= LOBJF_PLACED;
		lobj->mAddress = start;
		lobj->mRefAddress = start + mReloc;
		lobj->mRegion = this;

#if 1
		if (start != mStart + mUsed)
			mFreeChunks.Push( FreeChunk{ mStart + mUsed, start, mLastObject } );
#endif
		mUsed = end - mStart;

		mLastObject = lobj;

		if (merge && lobj->mSuffix && !(lobj->mSuffix->mFlags & LOBJF_PLACED))
		{
			if (!Allocate(linker, lobj->mSuffix, true, false))
				return false;
		}

		return true;
	}

	return false;
}

void LinkerRegion::PlaceStackSection(LinkerSection* stackSection, LinkerSection* section)
{
	if (!section->mEnd && !(section->mFlags & LSECF_PLACED))
	{
		section->mFlags |= LSECF_PLACED;

		int	start = stackSection->mEnd;

		for (int i = 0; i < section->mSections.Size(); i++)
		{
			PlaceStackSection(stackSection, section->mSections[i]);
			if (section->mSections[i]->mStart < start)
			{
				start = section->mSections[i]->mStart;
				section->mStart = start;
			}
		}

		section->mStart = start;
		section->mEnd = start;

		for (int i = 0; i < section->mObjects.Size(); i++)
		{
			LinkerObject* lobj = section->mObjects[i];
			if (lobj->mFlags & LOBJF_REFERENCED)
			{
				section->mStart -= lobj->mSize;
				section->mSize += lobj->mSize;

				lobj->mFlags |= LOBJF_PLACED;
				lobj->mAddress = section->mStart;
				lobj->mRefAddress = section->mStart + mReloc;
				lobj->mRegion = this;
			}
		}

		if (stackSection->mStart > section->mStart)
			stackSection->mStart = section->mStart;
	}
}

void Linker::CopyObjects(bool inlays)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* obj = mObjects[i];
		if (obj->mType == LOT_SECTION_START)
		{
			obj->mAddress = obj->mSection->mStart;
			obj->mRefAddress = obj->mAddress + (obj->mRegion ? obj->mRegion->mReloc : 0);
		}
		else if (obj->mType == LOT_SECTION_END)
		{
			obj->mAddress = obj->mSection->mEnd;
			obj->mRefAddress = obj->mAddress + (obj->mRegion ? obj->mRegion->mReloc : 0);
		}
		else if (obj->mFlags & LOBJF_REFERENCED)
		{
			if (inlays)
			{
				if (obj->mRegion && obj->mRegion->mInlayObject)
				{
					LinkerObject* iobj = obj->mRegion->mInlayObject;
					if (!iobj->mMemory)
					{
						iobj->mMemory = new uint8[0x10000];
						memset(iobj->mMemory, 0, 0x10000);
					}

					memcpy(iobj->mMemory + obj->mAddress, obj->mData, obj->mSize);
					if (obj->mAddress < iobj->mStartUsed)
						iobj->mStartUsed = obj->mAddress;
					if (obj->mAddress + obj->mSize > iobj->mEndUsed)
						iobj->mEndUsed = obj->mAddress + obj->mSize;
				}
			}
			else
			{
				if (!obj->mRegion)
				{
					mErrors->Error(obj->mLocation, ERRR_INSUFFICIENT_MEMORY, "Could not place object", obj->mIdent);

					int avail = 0;
					for (int i = 0; i < mRegions.Size(); i++)
					{
						if (mRegions[i]->mSections.Contains(obj->mSection))
						{
							if (mRegions[i]->mEnd - mRegions[i]->mStart - mRegions[i]->mUsed > avail)
								avail = mRegions[i]->mEnd - mRegions[i]->mStart - mRegions[i]->mUsed;
						}
					}
					char buffer[200];
					sprintf_s(buffer, "Size %d Available %d in section", obj->mSize, avail);
					mErrors->Error(obj->mLocation, EINFO_SIZE, buffer, obj->mSection->mIdent);
				}
				else if (obj->mRegion->mInlayObject)
					;
				else if (obj->mRegion && obj->mRegion->mCartridgeBanks != 0)
				{
					for (int i = 0; i < 64; i++)
					{
						if (obj->mRegion->mCartridgeBanks & (1ULL << i))
						{
							mCartridgeBankUsed[i] = true;
							memcpy(mCartridge[i] + obj->mAddress, obj->mData, obj->mSize);
							if (obj->mAddress < mCartridgeBankStart[i])
								mCartridgeBankStart[i] = obj->mAddress;
							if (obj->mAddress + obj->mSize > mCartridgeBankEnd[i])
								mCartridgeBankEnd[i] = obj->mAddress + obj->mSize;
						}
					}
				}
				else if (obj->mSection->mType == LST_DATA)
				{
					memcpy(mMemory + obj->mAddress, obj->mData, obj->mSize);
				}
			}
		}
	}
}

void Linker::CollectBreakpoints(void)
{
	for (int i = 0; i < mReferences.Size(); i++)
	{
		LinkerReference* ref = mReferences[i];
		if (ref->mFlags & LREF_BREAKPOINT)
		{
			LinkerObject* obj = ref->mObject;
			if (obj->mFlags & LOBJF_REFERENCED)
			{
				if (obj->mRegion)
				{
					if (obj->mRegion->mCartridgeBanks)
					{
					}
					else
					{
						mBreakpoints.Push(obj->mAddress + ref->mOffset);
					}
				}
			}
		}
	}

}

void Linker::PatchReferences(bool inlays)
{
	for (int i = 0; i < mReferences.Size(); i++)
	{
		LinkerReference* ref = mReferences[i];
		LinkerObject* obj = ref->mObject;
		if (obj->mFlags & LOBJF_REFERENCED)
		{
			if (obj->mRegion)
			{
				LinkerObject* robj = ref->mRefObject;

				int			raddr = robj->mRefAddress + ref->mRefOffset;
				uint8* dp;

				if (inlays)
				{
					if (obj->mRegion->mInlayObject)
					{
						LinkerObject* iobj = obj->mRegion->mInlayObject;

						dp = iobj->mMemory + obj->mAddress + ref->mOffset;

						if (ref->mFlags & LREF_LOWBYTE)
						{
							*dp++ = raddr & 0xff;
						}
						if (ref->mFlags & LREF_HIGHBYTE)
						{
							*dp++ = (raddr >> 8) & 0xff;
						}
						if (ref->mFlags & LREF_TEMPORARY)
							*dp += obj->mTemporaries[ref->mRefOffset];
					}
				}
				else if (!obj->mRegion->mInlayObject)
				{
					if (obj->mRegion->mCartridgeBanks)
					{
						for (int i = 0; i < 64; i++)
						{
							if (obj->mRegion->mCartridgeBanks & (1ULL << i))
							{
								dp = mCartridge[i] + obj->mAddress + ref->mOffset;

								if (ref->mFlags & LREF_LOWBYTE)
								{
									*dp++ = raddr & 0xff;
								}
								if (ref->mFlags & LREF_HIGHBYTE)
								{
									*dp++ = (raddr >> 8) & 0xff;
								}
								if (ref->mFlags & LREF_TEMPORARY)
									*dp += obj->mTemporaries[ref->mRefOffset];
							}
						}
					}
					else
					{
						dp = mMemory + obj->mAddress + ref->mOffset;

						if (ref->mFlags & LREF_LOWBYTE)
						{
							*dp++ = raddr & 0xff;
						}
						if (ref->mFlags & LREF_HIGHBYTE)
						{
							*dp++ = (raddr >> 8) & 0xff;
						}
						if (ref->mFlags & LREF_TEMPORARY)
							*dp += obj->mTemporaries[ref->mRefOffset];
					}
				}
			}
		}
	}
}

void Linker::PlaceObjects(bool retry)
{
	for (int i = 0; i < mRegions.Size(); i++)
	{
		LinkerRegion* lrgn = mRegions[i];
		for (int j = 0; j < lrgn->mSections.Size(); j++)
		{
			LinkerSection* lsec = lrgn->mSections[j];
			for (int k = 0; k < lsec->mObjects.Size(); k++)
			{
				LinkerObject* lobj = lsec->mObjects[k];
				if (lobj->mType != LOT_INLAY && (lobj->mFlags & LOBJF_REFERENCED) && !(lobj->mFlags & LOBJF_PLACED) && lrgn->Allocate(this, lobj, mCompilerOptions & COPT_OPTIMIZE_MERGE_CALLS, retry))
				{
					if (lobj->mIdent && lobj->mIdent->mString && (mCompilerOptions & COPT_VERBOSE2))
						printf("Placed object <%s> $%04x - $%04x\n", lobj->mIdent->mString, lobj->mAddress, lobj->mAddress + lobj->mSize);

					if (lobj->mAddress < lsec->mStart)
						lsec->mStart = lobj->mAddress;
					if (lobj->mAddress + lobj->mSize > lsec->mEnd)
						lsec->mEnd = lobj->mAddress + lobj->mSize;

					if (lsec->mType == LST_DATA && lsec->mEnd > lrgn->mNonzero)
						lrgn->mNonzero = lsec->mEnd;
				}
			}
		}
	}
}

void Linker::SortObjectsPartition(int l, int r)
{
	while (l < r)
	{
		int pi = (l + r) >> 1;

		LinkerObject* po = mObjects[pi];
		mObjects[pi] = mObjects[l];

		pi = l;
		for (int i = l + 1; i < r; i++)
		{
			if (mObjects[i]->IsBefore(po))
			{
				mObjects[pi++] = mObjects[i];
				mObjects[i] = mObjects[pi];
			}
		}
		mObjects[pi] = po;

		SortObjectsPartition(l, pi);
		l = pi + 1;
	}
}

void Linker::SortObjects(void)
{
	SortObjectsPartition(0, mObjects.Size());
}

void Linker::Link(void)
{
	if (mErrors->mErrorCount == 0)
	{

		for (int i = 0; i < mSections.Size(); i++)
		{
			LinkerSection* lsec = mSections[i];
			lsec->mStart = 0x10000;
			lsec->mEnd = 0x0000;
		}

		// Move objects into regions
		PlaceObjects(false);

		// Retry for alignment
		PlaceObjects(true);

		// Place stack segment

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			for (int j = 0; j < lrgn->mSections.Size(); j++)
			{
				LinkerSection* lsec = lrgn->mSections[j];

				if (lsec->mType == LST_STACK)
				{
					lsec->mStart = lsec->mEnd = lrgn->mEnd;
					lrgn->mEnd = lsec->mStart - lsec->mSize;

					for (int i = 0; i < lsec->mSections.Size(); i++)
						lrgn->PlaceStackSection(lsec, lsec->mSections[i]);

					if (lsec->mStart < lrgn->mEnd)
					{
						Location	loc;
						mErrors->Error(loc, ERRR_INSUFFICIENT_MEMORY, "Static stack usage exceeds stack segment");
					}

					lsec->mEnd = lsec->mStart;
					lsec->mStart = lrgn->mEnd;

					if (lsec->mStart < lrgn->mStart + lrgn->mUsed)
					{
						Location	loc;
						mErrors->Error(loc, ERRR_INSUFFICIENT_MEMORY, "Cannot place stack section");
					}
				}
			}
		}

		CopyObjects(true);
		PatchReferences(true);

		// Move inlays into regions

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			if (lrgn->mInlayObject)
			{
				LinkerObject* iobj = lrgn->mInlayObject;
				
				int size = CompressLZO(mWorkspace, iobj->mMemory + iobj->mStartUsed, iobj->mEndUsed - iobj->mStartUsed);
				iobj->AddData(mWorkspace, size);
				iobj->mType = LOT_DATA;
			}
		}

		PlaceObjects(false);

		// Calculate BSS storage

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			for (int j = 0; j < lrgn->mSections.Size(); j++)
			{
				LinkerSection* lsec = lrgn->mSections[j];
				if (lsec->mType == LST_BSS && lsec->mStart < lrgn->mNonzero)
					lsec->mStart = lrgn->mNonzero;
				if (lsec->mEnd < lsec->mStart)
					lsec->mEnd = lsec->mStart;
			}
		}

		mProgramStart = 0xffff;
		mProgramEnd = 0x0000;

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];

			if (lrgn->mNonzero && lrgn->mCartridgeBanks == 0 && !lrgn->mInlayObject)
			{
				if (lrgn->mStart < mProgramStart)
					mProgramStart = lrgn->mStart;
				if (lrgn->mNonzero > mProgramEnd)
					mProgramEnd = lrgn->mNonzero;
			}
		}

		// Now expand the heap section to cover the remainder of the region

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			for (int j = 0; j < lrgn->mSections.Size(); j++)
			{
				LinkerSection* lsec = lrgn->mSections[j];

				if (lsec->mType == LST_HEAP)
				{
					lsec->mStart = (lrgn->mStart + lrgn->mUsed + 7) & ~7;
					lsec->mEnd = lrgn->mEnd & ~7;

					if (lsec->mStart + lsec->mSize > lsec->mEnd)
					{
						Location	loc;
						mErrors->Error(loc, ERRR_INSUFFICIENT_MEMORY, "Cannot place heap section");
					}
				}
			}
		}

		// Second patch of references

		CopyObjects(false);
		PatchReferences(false);
		CollectBreakpoints();

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* oi = mObjects[i];

			if (oi->mSection->mType == LST_DATA && (oi->mFlags & LOBJF_PLACED) && oi->mRegion)
			{
				for (int j = i + 1; j < mObjects.Size(); j++)
				{
					LinkerObject* oj = mObjects[j];

					if (oj->mSection->mType == LST_DATA && (oj->mFlags & LOBJF_PLACED) && oj->mRegion)
					{
						if (oj->mAddress < oi->mAddress + oi->mSize && oi->mAddress < oj->mAddress + oj->mSize && (oj->mRegion->mCartridgeBanks & oi->mRegion->mCartridgeBanks))
						{
							mErrors->Error(oi->mLocation, EERR_OVERLAPPING_DATA_SECTIONS, "Overlapping data section", oi->mIdent);
							mErrors->Error(oj->mLocation, EERR_OVERLAPPING_DATA_SECTIONS, "Overlapping data section", oj->mIdent);
						}
					}
				}
			}
		}
	}

	SortObjects();
}

static const char * LinkerObjectTypeNames[] = 
{
	"NONE",
	"PAD",
	"BASIC",
	"BYTE_CODE",
	"NATIVE_CODE",
	"RUNTIME",
	"DATA",
	"BSS",
	"HEAP",
	"STACK",
	"INLAY",
	"START",
	"END"
};

static const char* LinkerSectionTypeNames[] = {
	"NONE",
	"DATA",
	"BSS",
	"HEAP",
	"STACK",
	"SSTACK",
	"ZEROPAGE"
};

bool Linker::WriteBinFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		ptrdiff_t	done = fwrite(mMemory + mProgramStart, 1, mProgramEnd - mProgramStart, file);
		fclose(file);
		return done == mProgramEnd - mProgramStart;
	}
	else
		return false;
}

bool Linker::WriteNesFile(const char* filename, TargetMachine machine)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		char header[16] = { 0x4e, 0x45, 0x53, 0x1a, 0x02, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00 };

		switch (machine)
		{
		case TMACH_NES:
			header[6] = 0x08;
			break;
		case TMACH_NES_NROM_H:
			header[6] = 0x00;
			break;
		case TMACH_NES_NROM_V:
			header[6] = 0x01;
			break;
		case TMACH_NES_MMC1:
			header[4] = 16;
			header[5] = 16;
			header[6] = 0x10;
			break;
		case TMACH_NES_MMC3:
			header[4] = 32;
			header[5] = 32;
			header[6] = 0x48;
			break;
		}

		ptrdiff_t done = fwrite(header, 1, 16, file);

		switch (machine)
		{
		case TMACH_NES:
		case TMACH_NES_NROM_H:
		case TMACH_NES_NROM_V:
			fwrite(mCartridge[0] + 0x8000, 1, 0x8000, file);
			fwrite(mCartridge[0], 1, 0x2000, file);
			break;
		case TMACH_NES_MMC1:
			for(int i=0; i<15; i++)
				fwrite(mCartridge[i] + 0x8000, 1, 0x4000, file);
			fwrite(mCartridge[15] + 0xc000, 1, 0x4000, file);
			for (int i = 0; i < 16; i++)
				fwrite(mCartridge[i], 1, 0x2000, file);
			break;
		case TMACH_NES_MMC3:
			for (int i = 0; i < 31; i++)
				fwrite(mCartridge[i] + 0x8000, 1, 0x4000, file);
			fwrite(mCartridge[31] + 0xc000, 1, 0x4000, file);
			for (int i = 0; i < 32; i++)
				fwrite(mCartridge[i], 1, 0x2000, file);
			break;
		}

		fclose(file);
		return done == 16;
	}
	else
		return false;
}

bool Linker::WritePrgFile(DiskImage* image, const char* filename)
{
	if (image->OpenFile(filename))
	{
		mMemory[mProgramStart - 2] = mProgramStart & 0xff;
		mMemory[mProgramStart - 1] = mProgramStart >> 8;

		image->WriteBytes(mMemory + mProgramStart - 2, mProgramEnd - mProgramStart + 2);
		image->CloseFile();

		for (int i = 0; i < mOverlays.Size(); i++)
		{
			if (image->OpenFile(mOverlays[i]->mIdent->mString))
			{
				int	b = mOverlays[i]->mBank;
				int	s = mCartridgeBankStart[b];

				mCartridge[b][s - 2] = s & 0xff;
				mCartridge[b][s - 1] = s >> 8;

				image->WriteBytes(mCartridge[b] + s - 2, mCartridgeBankEnd[b] - s + 2);

				image->CloseFile();
			}
			else
				return false;
		}

		return true;
	}

	return false;
}

bool Linker::WriteXexFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		// prefix
		fputc(0xff, file); fputc(0xff, file);

		// first segment
		fputc(mProgramStart & 0xff, file);
		fputc(mProgramStart >> 8, file);
		fputc((mProgramEnd - 1) & 0xff, file);
		fputc((mProgramEnd - 1) >> 8, file);

		ptrdiff_t	done = fwrite(mMemory + mProgramStart, 1, mProgramEnd - mProgramStart, file);

		fputc(0xe0, file);
		fputc(0x02, file);
		fputc(0xe1, file);
		fputc(0x02, file);
		fputc(mProgramStart & 0xff, file);
		fputc(mProgramStart >> 8, file);

		fclose(file);
		return true;
	}
	else
		return false;
}

bool Linker::WritePrgFile(const char* filename, const char* pathname)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		mMemory[mProgramStart - 2] = mProgramStart & 0xff;
		mMemory[mProgramStart - 1] = mProgramStart >> 8;

		ptrdiff_t	done = fwrite(mMemory + mProgramStart - 2, 1, mProgramEnd - mProgramStart + 2, file);
		fclose(file);
		if (done == mProgramEnd - mProgramStart + 2)
		{
			for (int i = 0; i < mOverlays.Size(); i++)
			{
				char	ofname[200];
				strcpy_s(ofname, pathname);
				strcat_s(ofname, mOverlays[i]->mIdent->mString);
				strcat_s(ofname, ".prg");

				fopen_s(&file, ofname, "wb");
				if (file)
				{
					int	b = mOverlays[i]->mBank;
					int	s = mCartridgeBankStart[b];

					mCartridge[b][s - 2] = s & 0xff;
					mCartridge[b][s - 1] = s >> 8;

					fwrite(mCartridge[b] + s - 2, 1, mCartridgeBankEnd[b] - s + 2, file);
					fclose(file);
				}
				else
					return false;
			}

			return true;
		}
	}

	return false;
}

static int memlzcomp(uint8 * dp, const uint8 * sp, int size)
{
	int	pos = 0, csize = 0;
	while (pos < size)
	{
		int	pi = 0;
		while (pi < 127 && pos < size)
		{
			int	bi = pi, bj = 0;
			for (int i = 1; i < (pos < 255 ? pos : 255); i++)
			{
				int j = 0;
				while (j < 127 && pos + j < size && sp[pos - i + j] == sp[pos + j])
					j++;

				if (j > bj)
				{
					bi = i;
					bj = j;
				}
			}

			if (bj >= 4)
			{
				if (pi > 0)
				{
					dp[csize++] = pi;
					for (int i = 0; i < pi; i++)
						dp[csize++] = sp[pos - pi + i];
					pi = 0;
				}

				dp[csize++] = 128 + bj;
				dp[csize++] = bi;
				pos += bj;
			}
			else
			{
				pos++;
				pi++;
			}
		}

		if (pi > 0)
		{
			dp[csize++] = pi;
			for (int i = 0; i < pi; i++)
				dp[csize++] = sp[pos - pi + i];
		}
	}

	dp[csize++] = 0;

	return csize;
}

static uint16 flip16(uint16 w)
{
	return (w >> 8) | (w << 8);
}

static uint32 flip32(uint32 d)
{
	return uint32(flip16(uint16(d >> 16))) | (uint32(flip16(uint16(d))) << 16);
}

bool Linker::WriteCrtFile(const char* filename, uint16 id)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		struct CRIHeader
		{
			char	mSignature[16];
			uint32	mHeaderLength;
			uint16	mVersion;
			uint8	mIDHi, mIDLo;
			uint8	mExrom, mGameLine;
			uint8	mPad[6];
			char	mName[32];
		}	criHeader = { 0 };

		memcpy(criHeader.mSignature, "C64 CARTRIDGE   ", 16);
		criHeader.mHeaderLength = 0x40000000;
		criHeader.mVersion = 0x0001;
		criHeader.mIDHi = uint8(id >> 8);
		criHeader.mIDLo = uint8(id & 0xff);

		if (mCompilerOptions & COPT_TARGET_CRT8)
		{
			criHeader.mExrom = 0;
			criHeader.mGameLine = 1;
		}
		else if (mCompilerOptions & COPT_TARGET_CRT16)
		{
			criHeader.mExrom = 0;
			criHeader.mGameLine = 0;
		}
		else
		{
			criHeader.mExrom = 0;
			criHeader.mGameLine = 0;
		}

		memset(criHeader.mName, 0, 32);
		strcpy_s(criHeader.mName, "OSCAR");

		fwrite(&criHeader, sizeof(CRIHeader), 1, file);

		struct CHIPHeader
		{
			char	mSignature[4];
			uint32	mPacketLength;
			uint16	mChipType, mBankNumber, mLoadAddress, mImageSize;
		}	chipHeader = { 0 };


		if (mCompilerOptions & COPT_TARGET_CRT_EASYFLASH) // EASYFLASH
		{
			memcpy(chipHeader.mSignature, "CHIP", 4);
			chipHeader.mPacketLength = 0x10200000;
			chipHeader.mChipType = 0;
			chipHeader.mBankNumber = 0;
			chipHeader.mImageSize = 0x0020;

			uint8 bootmem[0x4000];

			memset(bootmem, 0, 0x4000);

			if (mCartridgeBankUsed[0])
				memcpy(bootmem, mCartridge[0] + 0x8000, 0x4000);

			LinkerRegion* mainRegion = FindRegion(Ident::Unique("main"));
			LinkerRegion* startupRegion = FindRegion(Ident::Unique("startup"));

			memcpy(bootmem, mMemory + startupRegion->mStart, startupRegion->mNonzero - startupRegion->mStart);
			int usedlz = memlzcomp(bootmem + 0x0100, mMemory + mainRegion->mStart, mainRegion->mNonzero - mainRegion->mStart);

			Location	loc;

			if (usedlz > 0x03e00)
			{
				mErrors->Error(loc, ERRR_INSUFFICIENT_MEMORY, "Can not fit main region into first ROM bank");
				fclose(file);
				return false;
			}

			bootmem[0x3ffc] = 0x00;
			bootmem[0x3ffd] = 0xff;

			uint8	bootcode[] = {
				0xa9, 0x87,
				0x8d, 0x02, 0xde,
				0xa9, 0x00,
				0x8d, 0x00, 0xde,
				0x6c, 0xfc, 0xff
			};

			int j = 0x3f00;
			for (int i = 0; i < sizeof(bootcode); i++)
			{
				bootmem[j++] = 0xa9;
				bootmem[j++] = bootcode[i];
				bootmem[j++] = 0x8d;
				bootmem[j++] = i;
				bootmem[j++] = 0x04;
			}
			bootmem[j++] = 0x4c;
			bootmem[j++] = 0x00;
			bootmem[j++] = 0x04;

			chipHeader.mLoadAddress = 0x0080;
			fwrite(&chipHeader, sizeof(chipHeader), 1, file);
			fwrite(bootmem, 1, 0x2000, file);

			chipHeader.mLoadAddress = 0x00e0;
			fwrite(&chipHeader, sizeof(chipHeader), 1, file);
			fwrite(bootmem + 0x2000, 1, 0x2000, file);

			mCartridgeBankUsed[0] = true;
			mCartridgeBankStart[0] = 0x8000;
			mCartridgeBankEnd[0] = 0x8000 + usedlz + 0x200;

			for (int i = 1; i < 64; i++)
			{
				if (mCartridgeBankUsed[i])
				{
					chipHeader.mBankNumber = i << 8;

					chipHeader.mLoadAddress = 0x0080;
					fwrite(&chipHeader, sizeof(chipHeader), 1, file);
					fwrite(mCartridge[i] + 0x8000, 1, 0x2000, file);

					chipHeader.mLoadAddress = 0x00a0;
					fwrite(&chipHeader, sizeof(chipHeader), 1, file);
					fwrite(mCartridge[i] + 0xa000, 1, 0x2000, file);
				}
			}
		}
		else if (mCompilerOptions & COPT_TARGET_CRT8)
		{
			int	numBanks = 64;
			while (numBanks > 1 && !mCartridgeBankUsed[numBanks - 1])
				numBanks--;

			memcpy(chipHeader.mSignature, "CHIP", 4);
			chipHeader.mPacketLength = flip32(0x10 + 0x2000);
			chipHeader.mChipType = 0;
			chipHeader.mBankNumber = 0;
			chipHeader.mImageSize = flip16(0x2000);

			for (int i = 0; i < numBanks; i++)
			{
				if (mCartridgeBankUsed[i])
				{
					chipHeader.mBankNumber = flip16(uint16(i));

					chipHeader.mLoadAddress = flip16(0x8000);
					fwrite(&chipHeader, sizeof(chipHeader), 1, file);
					fwrite(mCartridge[i] + 0x8000, 1, 0x2000, file);
				}
			}
		}
		else if (mCompilerOptions & COPT_TARGET_CRT16)
		{
			int	numBanks = 64;
			while (numBanks > 1 && !mCartridgeBankUsed[numBanks - 1])
				numBanks--;

			memcpy(chipHeader.mSignature, "CHIP", 4);
			chipHeader.mPacketLength = flip32(0x10 + 0x4000);
			chipHeader.mChipType = 0;
			chipHeader.mBankNumber = 0;
			chipHeader.mImageSize = flip16(0x4000);

			for (int i = 0; i < numBanks; i++)
			{
				if (mCartridgeBankUsed[i])
				{
					chipHeader.mBankNumber = flip16(uint16(i));

					chipHeader.mLoadAddress = flip16(0x8000);
					fwrite(&chipHeader, sizeof(chipHeader), 1, file);
					fwrite(mCartridge[i] + 0x8000, 1, 0x4000, file);
				}
			}
		}

		fclose(file);
		return true;
	}
	else
		return false;
}


bool Linker::WriteMapFile(const char* filename)
{
	bool	banked = mCartridgeBankUsed[0];

	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		fprintf(file, "sections\n");
		for (int i = 0; i <  mSections.Size(); i++)
		{
			LinkerSection* lsec = mSections[i];

			fprintf(file, "%04x - %04x : %s, %s\n", lsec->mStart, lsec->mEnd, LinkerSectionTypeNames[lsec->mType], lsec->mIdent->mString);
		}

		fprintf(file, "\nregions\n");

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion	* lrgn = mRegions[i];

			fprintf(file, "%04x - %04x : %04x, %04x, %s\n", lrgn->mStart, lrgn->mEnd, lrgn->mNonzero, lrgn->mUsed, lrgn->mIdent->mString);
		}

		fprintf(file, "\nobjects\n");

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if (obj->mFlags & LOBJF_REFERENCED)
			{
				if (obj->mRegion)
				{
					if (banked)
					{
						int k = 0;
						while (k < 64 && !(obj->mRegion->mCartridgeBanks & (1ull << k)))
							k++;
						if (k < 64)
							fprintf(file, "%02x:", k);
						else
							fprintf(file, "--:");
					}
				}

				if (obj->mIdent)
					fprintf(file, "%04x - %04x : %s, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, obj->mIdent->mString, LinkerObjectTypeNames[obj->mType], obj->mSection->mIdent->mString);
				else
					fprintf(file, "%04x - %04x : *, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, LinkerObjectTypeNames[obj->mType], obj->mSection->mIdent->mString);
			}
		}

		if (banked)
		{
			fprintf(file, "\nbanks\n");

			for (int i = 0; i < 64; i++)
			{
				if (mCartridgeBankUsed[i])
					fprintf(file, "%02d : %04x .. %04x (%04x)\n", i, mCartridgeBankStart[i], mCartridgeBankEnd[i], mCartridgeBankEnd[i] - mCartridgeBankStart[i]);
			}
		}

		fprintf(file, "\nobjects by size\n");
		ExpandingArray<const LinkerObject*>	so;
		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if ((obj->mFlags & LOBJF_REFERENCED) && obj->mIdent)
			{
				int k = so.Size();
				so.Push(obj);
				while (k > 0 && so[k - 1]->mSize < obj->mSize)
				{
					so[k] = so[k - 1];
					k--;
				}
				so[k] = obj;
			}
		}

		for (int i = 0; i < so.Size(); i++)
		{
			const LinkerObject* obj = so[i];

			if (obj->mRegion)
			{
				if (banked)
				{
					int k = 0;
					while (k < 64 && !(obj->mRegion->mCartridgeBanks & (1ull << k)))
						k++;
					if (k < 64)
						fprintf(file, "%02x:", k);
					else
						fprintf(file, "--:");
				}
			}

			fprintf(file, "%04x (%04x) : %s, %s:%s\n", obj->mAddress, obj->mSize, obj->mIdent->mString, LinkerObjectTypeNames[obj->mType], obj->mSection->mIdent->mString);
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

int Linker::TranslateMlbAddress(int address, int bank, TargetMachine machine)
{
	switch (machine)
	{
	default:
	case TMACH_NES:
	case TMACH_NES_NROM_H:
	case TMACH_NES_NROM_V:
		return address - 0x8000;
	case TMACH_NES_MMC1:
		if (bank == 15)
			return 15 * 0x4000 + address - 0xc000;
		else
			return bank * 0x4000 + address - 0x8000;
	case TMACH_NES_MMC3:
		if (bank == 31)
			return 31 * 0x4000 + address - 0xc000;
		else
			return bank * 0x4000 + address - 0x8000;
	}
}

bool Linker::WriteMlbFile(const char* filename, TargetMachine machine)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		fprintf(file, "R:%02x-%02x:__ACCU\n", BC_REG_ACCU, BC_REG_ACCU + 3);
		fprintf(file, "R:%02x-%02x:__ADDR\n", BC_REG_ADDR, BC_REG_ADDR + 1);
		fprintf(file, "R:%02x-%02x:__IP\n", BC_REG_IP, BC_REG_IP + 1);
		fprintf(file, "R:%02x-%02x:__SP\n", BC_REG_STACK, BC_REG_STACK + 1);
		fprintf(file, "R:%02x-%02x:__FP\n", BC_REG_LOCALS, BC_REG_LOCALS + 1);
		fprintf(file, "R:%02x-%02x:__P\n", BC_REG_FPARAMS, BC_REG_FPARAMS_END - 1);
		fprintf(file, "R:%02x-%02x:__T\n", BC_REG_TMP, 0x7f);

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if ((obj->mFlags & LOBJF_REFERENCED) && obj->mIdent && obj->mSize > 0)
			{
				int bank = -1;
				if (obj->mRegion->mCartridgeBanks)
				{
					do { bank++; } while (!((1ULL << bank) & obj->mRegion->mCartridgeBanks));
				}

				if (obj->mSection->mType == LST_BSS)
				{
					if (obj->mRanges.Size() > 0)
					{
						for(int i=0; i<obj->mRanges.Size(); i++)
							fprintf(file, "R:%04x-%04x:%s@%s\n", obj->mAddress + obj->mRanges[i].mOffset, obj->mAddress + obj->mRanges[i].mOffset + obj->mRanges[i].mSize - 1, obj->mIdent->mString, obj->mRanges[i].mIdent->mString);
					}
					if (obj->mSize > 1)
						fprintf(file, "R:%04x-%04x:%s\n", obj->mAddress, obj->mAddress + obj->mSize - 1, obj->mIdent->mString);
					else
						fprintf(file, "R:%04x:%s\n", obj->mAddress, obj->mIdent->mString);
				}
				else if (obj->mType == LOT_DATA)
				{
					if (obj->mAddress >= 0x8000)
					{
						if (obj->mRanges.Size() > 0)
						{
							for (int i = 0; i < obj->mRanges.Size(); i++)
								fprintf(file, "P:%04x-%04x:%s@%s\n", TranslateMlbAddress(obj->mAddress + obj->mRanges[i].mOffset, bank, machine), TranslateMlbAddress(obj->mAddress + obj->mRanges[i].mOffset + obj->mRanges[i].mSize - 1, bank, machine), obj->mIdent->mString, obj->mRanges[i].mIdent->mString);
						}
						fprintf(file, "P:%04x-%04x:%s\n", TranslateMlbAddress(obj->mAddress, bank, machine), TranslateMlbAddress(obj->mAddress + obj->mSize - 1, bank, machine), obj->mIdent->mString);
					}
				}
				else if (obj->mType == LOT_NATIVE_CODE)
				{
					if (obj->mAddress >= 0x8000)
					{
						if (obj->mRanges.Size() > 0)
						{
							for (int i = 0; i < obj->mRanges.Size(); i++)
								fprintf(file, "P:%04x:%s@%s\n", TranslateMlbAddress(obj->mAddress + obj->mRanges[i].mOffset, bank, machine), obj->mIdent->mString, obj->mRanges[i].mIdent->mString);
						}

						fprintf(file, "P:%04x:%s\n", TranslateMlbAddress(obj->mAddress, bank, machine), obj->mIdent->mString);
					}
				}
			}
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

bool Linker::WriteDbjFile(FILE* file)
{
	fprintf(file, "\t\"memory\": [");

	bool first = true;
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* obj = mObjects[i];

		if (obj->mFlags & LOBJF_REFERENCED)
		{
			if (obj->mIdent)
			{
				if (!first)
					fprintf(file, ",\n");
				first = false;

				fprintf(file, "\t\t{\"name\": \"%s\", \"xname\": \"%s\", \"start\": %d, \"end\": %d, \"type\": \"%s\", \"source\": \"%s\", \"line\": %d }",
					obj->mIdent->mString, obj->mFullIdent->mString, obj->mAddress, obj->mAddress + obj->mSize, LinkerObjectTypeNames[obj->mType],
					obj->mLocation.mFileName, obj->mLocation.mLine);
			}
		}
	}

	fprintf(file, "]");


	return true;
}

bool Linker::WriteLblFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if (obj->mFlags & LOBJF_REFERENCED)
			{
				if (obj->mIdent)
					fprintf(file, "al %04x .%s\n", obj->mAddress, obj->mIdent->mString);
			}
		}

		for (int i = 0; i < mBreakpoints.Size(); i++)
		{
			fprintf(file, "break %04x\n", mBreakpoints[i]);
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

bool Linker::WriteAsmFile(const char* filename, const char* version)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		fprintf(file, "; Compiled with %s\n", version);

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if (obj->mFlags & LOBJF_REFERENCED)
			{
				switch (obj->mType)
				{
				case LOT_BYTE_CODE:
					mByteCodeDisassembler.Disassemble(file, mMemory, -1, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this);
					break;
				case LOT_NATIVE_CODE:
					if (!obj->mRegion)
						mNativeDisassembler.Disassemble(file, obj->mData, -1, 0, obj->mSize, obj->mProc, obj->mIdent, this, obj->mFullIdent);
					else if (obj->mRegion->mCartridgeBanks)
					{
						int i = 0;
						while (!(obj->mRegion->mCartridgeBanks & (1ULL << i)))
							i++;
						mNativeDisassembler.Disassemble(file, mCartridge[i], i, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj->mFullIdent);
					}
					else if (obj->mRegion->mInlayObject)
						mNativeDisassembler.Disassemble(file, obj->mRegion->mInlayObject->mMemory, 0xa0, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj->mFullIdent);
					else
						mNativeDisassembler.Disassemble(file, mMemory, -1, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj->mFullIdent);
					break;
				case LOT_DATA:
					if (!obj->mRegion)
						mNativeDisassembler.DumpMemory(file, obj->mData, -1, 0, obj->mSize, obj->mProc, obj->mIdent, this, obj);
					else if (obj->mRegion->mCartridgeBanks)
					{
						int i = 0;
						while (!(obj->mRegion->mCartridgeBanks & (1ULL << i)))
							i++;
						mNativeDisassembler.DumpMemory(file, mCartridge[i], i, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj);
					}
					else if (obj->mRegion->mInlayObject)
						mNativeDisassembler.DumpMemory(file, obj->mRegion->mInlayObject->mMemory, 0xa0, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj);
					else
						mNativeDisassembler.DumpMemory(file, mMemory, -1, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent, this, obj);
					break;
				}
			}
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

