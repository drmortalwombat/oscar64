#pragma once

#include "Errors.h"
#include <stdio.h>
#include "MachineTypes.h"

class SourceFile
{
public:
	char			mFileName[MAXPATHLEN];

	SourceFile	*	mUp, * mNext;
	Location		mLocation;

	bool ReadLine(char* line);

	SourceFile(void);
	~SourceFile(void);

	bool Open(const char* name, const char * path);
	void Close(void);
protected:
	FILE* mFile;
};

class SourcePath
{
public:
	char			mPathName[MAXPATHLEN];

	SourcePath* mNext;

	SourcePath(const char* path);
	~SourcePath(void);
};

class Preprocessor
{
public:
	char		mLine[32768];

	Location	mLocation;
	Errors* mErrors;

	SourceFile* mSource, * mSourceList;
	SourcePath* mPaths;

	void AddPath(const char* path);
	bool NextLine(void);

	bool OpenSource(const char* name, bool local);
	bool CloseSource(void);

	Preprocessor(Errors * errors);
	~Preprocessor(void);
};
