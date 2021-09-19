#include "Linker.h"
#include <string.h>
#include <stdio.h>

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

int Linker::AddObject(const Location& location, const Ident* ident, const Ident* section, LinkerObjectType type)
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
	mObjects.Push(obj);
	return obj->mID;
}

void Linker::AddObjectData(int id, const uint8* data, int size)
{
	LinkerObject* obj = mObjects[id];
	obj->mSize = size;
	obj->mData = new uint8[size];
	memcpy(obj->mData, data, size);
}

uint8* Linker::AddObjectSpace(int id, int size)
{
	LinkerObject* obj = mObjects[id];
	obj->mSize = size;
	obj->mData = new uint8[size];
	memset(obj->mData, 0, size);
	return obj->mData;
}

void Linker::AttachObjectProcedure(int id, InterCodeProcedure* proc)
{
	LinkerObject* obj = mObjects[id];
	obj->mProc = proc;
}

void Linker::AddReference(const LinkerReference& ref)
{
	LinkerReference* nref = new LinkerReference(ref);
	mReferences.Push(nref);	
}

void Linker::Link(void)
{
	for (int i = 0; i < mObjects.Size(); i++)
	{
		LinkerObject* obj = mObjects[i];
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
			memcpy(mMemory + obj->mAddress, obj->mData, obj->mSize);
		}
		else
			mErrors->Error(obj->mLocation, "Out of space in section", obj->mSection->mString);
	}

	if (mErrors->mErrorCount == 0)
	{
		mProgramStart = 0xffff;
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
			obj->mAddress += obj->mLinkerSection->mStart;
		}

		for (int i = 0; i < mReferences.Size(); i++)
		{
			LinkerReference* ref = mReferences[i];
			LinkerObject* obj = mObjects[ref->mID];
			LinkerObject* robj = mObjects[ref->mRefID];

			int			raddr = robj->mAddress + ref->mRefOffset;
			uint8* dp = mMemory + obj->mAddress + ref->mOffset;

			if (ref->mLowByte)
				*dp++ = raddr & 0xff;
			if (ref->mHighByte)
				*dp++ = (raddr >> 8) & 0xff;

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

			if (obj->mIdent)
				fprintf(file, "%04x - %04x : %s, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, obj->mIdent->mString, LinkerObjectTypeNames[obj->mType], obj->mSection->mString);
			else
				fprintf(file, "%04x - %04x : *, %s:%s\n", obj->mAddress, obj->mAddress + obj->mSize, LinkerObjectTypeNames[obj->mType], obj->mSection->mString);
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

			switch (obj->mType)
			{
			case LOT_BYTE_CODE:
				mByteCodeDisassembler.Disassemble(file, mMemory, obj->mAddress, obj->mSize, obj->mProc);
				break;
			case LOT_NATIVE_CODE:
				mNativeDisassembler.Disassemble(file, mMemory, obj->mAddress, obj->mSize, obj->mProc);
				break;
			}
		}

		fclose(file);

		return true;
	}
	else
		return false;
}

