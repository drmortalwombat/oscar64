#include "Linker.h"
#include <string.h>
#include <stdio.h>


LinkerRegion::LinkerRegion(void)
	: mSections(nullptr)
{}

LinkerSection::LinkerSection(void)
	: mObjects(nullptr)
{}

void LinkerObject::AddData(const uint8* data, int size)
{
	mSize = size;
	mData = new uint8[size];
	memcpy(mData, data, size);
}

uint8* LinkerObject::AddSpace(int size)
{
	mSize = size;
	mData = new uint8[size];
	memset(mData, 0, size);
	return mData;
}

Linker::Linker(Errors* errors)
	: mErrors(errors), mSections(nullptr), mReferences(nullptr), mObjects(nullptr), mRegions(nullptr)
{

}

Linker::~Linker(void)
{

}


LinkerRegion* Linker::AddRegion(const Ident* region, int start, int end)
{
	LinkerRegion* lrgn = new LinkerRegion();
	lrgn->mIdent = region;
	lrgn->mStart = start;
	lrgn->mEnd = end;
	lrgn->mUsed = 0;
	mRegions.Push(lrgn);
	return lrgn;
}

LinkerSection* Linker::AddSection(const Ident* section, uint32 flags)
{
	LinkerSection* lsec = new LinkerSection;
	lsec->mIdent = section;
	mSections.Push(lsec);
	return lsec;

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

LinkerObject * Linker::AddObject(const Location& location, const Ident* ident, LinkerSection * section, LinkerObjectType type)
{
	LinkerObject* obj = new LinkerObject;
	obj->mLocation = location;
	obj->mID = mObjects.Size();
	obj->mType = type;
	obj->mData = nullptr;
	obj->mSize = 0;
	obj->mIdent = ident;
	obj->mSection = section;
	obj->mProc = nullptr;
	obj->mReferenced = false;
	obj->mPlaced = false;
	section->mObjects.Push(obj);
	mObjects.Push(obj);
	return obj;
}

void Linker::AddReference(const LinkerReference& ref)
{
	LinkerReference* nref = new LinkerReference(ref);
	mReferences.Push(nref);	
}

void Linker::ReferenceObject(LinkerObject* obj)
{
	if (!obj->mReferenced)
	{
		obj->mReferenced = true;
		for (int i = 0; i < mReferences.Size(); i++)
		{
			LinkerReference* ref = mReferences[i];
			if (ref->mObject == obj)
				ReferenceObject(ref->mRefObject);
		}
	}
}

void Linker::Link(void)
{
	if (mErrors->mErrorCount == 0)
	{

		// Move objects into regions

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			for (int j = 0; j < lrgn->mSections.Size(); j++)
			{
				LinkerSection* lsec = lrgn->mSections[j];
				for (int k = 0; k < lsec->mObjects.Size(); k++)
				{
					LinkerObject* lobj = lsec->mObjects[k];
					if (lobj->mReferenced && !lobj->mPlaced && lrgn->mStart + lrgn->mUsed + lobj->mSize <= lrgn->mEnd)
					{
						lobj->mPlaced = true;
						lobj->mAddress = lrgn->mStart + lrgn->mUsed;
						lrgn->mUsed += lobj->mSize;
					}
				}
			}
		}

		mProgramStart = 0x0801;
		mProgramEnd = 0x0801;

		int	address = 0;

		for (int i = 0; i < mRegions.Size(); i++)
		{
			LinkerRegion* lrgn = mRegions[i];
			address = lrgn->mStart + lrgn->mUsed;

			if (lrgn->mUsed && address > mProgramEnd)
				mProgramEnd = address;
		}

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];
			if (obj->mReferenced)
			{
				memcpy(mMemory + obj->mAddress, obj->mData, obj->mSize);
			}
		}

		for (int i = 0; i < mReferences.Size(); i++)
		{
			LinkerReference* ref = mReferences[i];
			LinkerObject* obj = ref->mObject;
			if (obj->mReferenced)
			{
				LinkerObject* robj = ref->mRefObject;

				int			raddr = robj->mAddress + ref->mRefOffset;
				uint8* dp = mMemory + obj->mAddress + ref->mOffset;

				if (ref->mLowByte)
					*dp++ = raddr & 0xff;
				if (ref->mHighByte)
					*dp++ = (raddr >> 8) & 0xff;
			}
		}
	}
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
	"STACK"
};

bool Linker::WritePrgFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		mMemory[mProgramStart - 2] = mProgramStart & 0xff;
		mMemory[mProgramStart - 1] = mProgramStart >> 8;

		int	done = fwrite(mMemory + mProgramStart - 2, 1, mProgramEnd - mProgramStart + 2, file);
		fclose(file);
		return done == mProgramEnd - mProgramStart + 2;
	}
	else
		return false;
}

bool Linker::WriteMapFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if (obj->mReferenced)
			{
				if (obj->mIdent)
					fprintf(file, "%04x - %04x : %s, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, obj->mIdent->mString, LinkerObjectTypeNames[obj->mType], obj->mSection->mIdent->mString);
				else
					fprintf(file, "%04x - %04x : *, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, LinkerObjectTypeNames[obj->mType], obj->mSection->mIdent->mString);
			}
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

bool Linker::WriteAsmFile(const char* filename)
{
	FILE* file;
	fopen_s(&file, filename, "wb");
	if (file)
	{
		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];

			if (obj->mReferenced)
			{
				switch (obj->mType)
				{
				case LOT_BYTE_CODE:
					mByteCodeDisassembler.Disassemble(file, mMemory, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent);
					break;
				case LOT_NATIVE_CODE:
					mNativeDisassembler.Disassemble(file, mMemory, obj->mAddress, obj->mSize, obj->mProc, obj->mIdent);
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

