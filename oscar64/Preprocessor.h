#pragma once

#include "Errors.h"
#include <stdio.h>
#include "MachineTypes.h"
#include "CompilerTypes.h"

class SourceStack
{
public:
	SourceStack* mUp;

	int				mFilePos;
	Location		mLocation;
};

class SourceFile
{
public:
	char			mFileName[MAXPATHLEN];

	SourceFile	*	mUp, * mNext;
	Location		mLocation;
	SourceStack	*	mStack;
	bool			mBinary;
	int				mLimit;

	bool ReadLine(char* line);

	SourceFile(void);
	~SourceFile(void);

	bool Open(const char* name, const char * path, bool binary = false);
	void Close(void);

	void Limit(int skip, int limit);

	bool PushSource(void);
	bool PopSource(void);
	bool DropSource(void);

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

	uint64		mCompilerOptions;

	void AddPath(const char* path);
	bool NextLine(void);

	bool OpenSource(const char* reason, const char* name, bool local);
	bool CloseSource(void);

	bool PushSource(void);
	bool PopSource(void);
	bool DropSource(void);

	bool EmbedData(const char* reason, const char* name, bool local, int skip, int limit);

	Preprocessor(Errors * errors);
	~Preprocessor(void);
};
