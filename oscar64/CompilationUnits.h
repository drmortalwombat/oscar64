#pragma once

#include "Declaration.h"
#include "Errors.h"

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

	DeclarationScope* mScope;
	CompilationUnit* mCompilationUnits, * mPendingUnits;

	Declaration* mStartup;
	Declaration* mByteCodes[128];

	DeclarationScope* mRuntimeScope;

	bool AddUnit(Location & location, const char* name, const char * from);
	CompilationUnit* PendingUnit(void);
protected:
	Errors* mErrors;
};

