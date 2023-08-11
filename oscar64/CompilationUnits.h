#pragma once

#include "Declaration.h"
#include "Errors.h"
#include "Linker.h"

class CompilationUnit
{
public:
	Location			mLocation;
	char				mFileName[200];
	CompilationUnit	*	mNext;
	bool				mCompiled;
};

class CompilationUnits
{
public:
	CompilationUnits(Errors * errors);
	~CompilationUnits(void);

	DeclarationScope* mScope, * mVTableScope, * mTemplateScope;
	CompilationUnit* mCompilationUnits, * mPendingUnits;

	Declaration* mStartup;
	Declaration* mByteCodes[256];
	GrowingArray<Declaration*>	mReferenced;

	DeclarationScope* mRuntimeScope;

	LinkerSection* mSectionCode, * mSectionData, * mSectionBSS, * mSectionHeap, * mSectionStack, * mSectionZeroPage, * mSectionLowCode, * mSectionBoot;
	Linker* mLinker;

	bool AddUnit(Location & location, const char* name, const char * from);
	CompilationUnit* PendingUnit(void);

	void AddReferenced(Declaration* ref);

protected:
	Errors* mErrors;
};

