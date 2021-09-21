#include "Linker.h"
#include <string.h>
#include <stdio.h>


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
	: mErrors(errors), mSections(nullptr), mReferences(nullptr), mObjects(nullptr)
{

}

Linker::~Linker(void)
{

}

int Linker::AddSection(const Ident* section, int start, int size)
{
	LinkerSection* lsec = new LinkerSection;
	lsec->mID = mSections.Size();
	lsec->mIdent = section;
	lsec->mStart = start;
	lsec->mSize = size;
	lsec->mUsed = 0;
	mSections.Push(lsec);
	return lsec->mID;
}

LinkerObject * Linker::AddObject(const Location& location, const Ident* ident, const Ident* section, LinkerObjectType type)
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
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* obj = mObjects[i];
		if (obj->mReferenced)
		{
			LinkerSection* lsec;
			int j = 0;
			while (j < mSections.Size() && !(mSections[j]->mIdent == obj->mSection && mSections[j]->mUsed + obj->mSize <= mSections[j]->mSize))
				j++;
			if (j < mSections.Size())
			{
				LinkerSection* lsec = mSections[j];
				obj->mLinkerSection = lsec;
				obj->mAddress = lsec->mUsed;
				lsec->mUsed += obj->mSize;
			}
			else
				mErrors->Error(obj->mLocation, EERR_OUT_OF_MEMORY, "Out of space in section", obj->mSection->mString);
		}
	}

	if (mErrors->mErrorCount == 0)
	{
		mProgramStart = 0x0801;
		mProgramEnd = 0x0801;

		int	address = 0;

		for (int i = 0; i < mSections.Size(); i++)
		{
			LinkerSection* lsec = mSections[i];
			if (lsec->mStart == 0)
				lsec->mStart = address;
			address = lsec->mStart + lsec->mUsed;

			if (lsec->mUsed > 0)
			{
				if (address > mProgramEnd)
					mProgramEnd = address;
			}
		}

		for (int i = 0; i < mObjects.Size(); i++)
		{
			LinkerObject* obj = mObjects[i];
			if (obj->mReferenced)
			{
				obj->mAddress += obj->mLinkerSection->mStart;
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
					fprintf(file, "%04x - %04x : %s, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, obj->mIdent->mString, LinkerObjectTypeNames[obj->mType], obj->mSection->mString);
				else
					fprintf(file, "%04x - %04x : *, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, LinkerObjectTypeNames[obj->mType], obj->mSection->mString);
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

