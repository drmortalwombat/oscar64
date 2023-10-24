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

enum SourceFileMode
{
	SFM_TEXT,
	SFM_BINARY,
	SFM_BINARY_WORD,
	SFM_BINARY_RLE,
	SFM_BINARY_LZO
};

enum SourceFileDecoder
{
	SFD_NONE,
	SFD_CTM_CHARS,
	SFD_CTM_CHAR_ATTRIB_1,
	SFD_CTM_CHAR_ATTRIB_2,
	SFD_CTM_TILES_8,
	SFD_CTM_TILES_16,
	SFD_CTM_MAP_8,
	SFD_CTM_MAP_16,
	SFD_SPD_SPRITES
};

class SourceFile
{
public:
	char			mFileName[MAXPATHLEN], mLocationFileName[MAXPATHLEN];

	SourceFile	*	mUp, * mNext;
	Location		mLocation;
	SourceStack	*	mStack;
	SourceFileMode	mMode;
	int				mLimit;

	uint8			mBuffer[512];
	int				mFill, mPos, mMemPos, mMemSize;
	uint8		*	mMemData;

	bool ReadLine(char* line);

	bool ReadLineRLE(char* line);
	bool ReadLineLZO(char* line);

	SourceFile(void);
	~SourceFile(void);

	bool Open(const char* name, const char * path, SourceFileMode mode = SFM_TEXT);
	void Close(void);

	void Limit(SourceFileDecoder decoder, int skip, int limit);

	bool PushSource(void);
	bool PopSource(void);
	bool DropSource(void);

protected:
	FILE* mFile;

	void ReadCharPad(SourceFileDecoder decoder);
	void ReadSpritePad(SourceFileDecoder decoder);
	int ReadChar(void);
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

	bool EmbedData(const char* reason, const char* name, bool local, int skip, int limit, SourceFileMode mode, SourceFileDecoder decoder);

	Preprocessor(Errors * errors);
	~Preprocessor(void);
};
