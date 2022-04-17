#include "Preprocessor.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

SourcePath::SourcePath(const char* path)
{
	strcpy_s(mPathName, path);
	char* p = mPathName;
	while (*p)
	{
		if (*p == '\\')
			*p = '/';
		p++;
	}
}

SourcePath::~SourcePath(void)
{

}

bool SourceFile::ReadLineRLE(char* line)
{
	assert(mFill >= 0 && mFill < 256);

	int	c;
	while (mLimit && mFill < 256 && (c = fgetc(mFile)) >= 0)
	{
		mLimit--;
		mBuffer[mFill++] = c;
	}

	assert(mFill >= 0 && mFill <= 256);

	if (mFill)
	{
		if (mFill >= 3 && mBuffer[0] == mBuffer[1] && mBuffer[1] == mBuffer[2])
		{
			int	cnt = 1;
			while (cnt < 64 && cnt < mFill && mBuffer[cnt] == mBuffer[cnt - 1])
				cnt++;

			if (cnt <= 8 && cnt < mFill)
			{
				int	rcnt = 1;
				int rep = 1;
				while (rcnt < 16 && cnt + rcnt < mFill && rep < 3)
				{
					if (mBuffer[cnt + rcnt] == mBuffer[cnt + rcnt - 1])
						rep++;
					else
						rep = 1;
					rcnt++;
				}
				if (cnt + rcnt < mFill && rep >= 3)
					rcnt -= rep;

				if (rcnt > 0)
				{
					sprintf_s(line, 1024, "0x%02x, 0x%02x, ", 0x80 + ((cnt - 1) << 4) + (rcnt - 1), (unsigned char)mBuffer[0]);

					assert(mFill >= 0 && mFill <= 256);

					for (int i = 0; i < rcnt; i++)
					{
						char	buffer[16];
						sprintf_s(buffer, 16, "0x%02x, ", (unsigned char)mBuffer[cnt + i]);

						assert(mFill >= 0 && mFill <= 256);

						strcat_s(line, 1024, buffer);

						assert(mFill >= 0 && mFill <= 256);
					}

					assert(mFill >= cnt + rcnt);
					assert(mFill >= 0 && mFill <= 256);

					memmove(mBuffer, mBuffer + cnt + rcnt, mFill - cnt - rcnt);
					mFill -= cnt + rcnt;

					assert(mFill >= 0 && mFill < 256);
				}
				else
				{
					sprintf_s(line, 1024, "0x%02x, 0x%02x, ", 0x00 + (cnt - 1), (unsigned char)mBuffer[0]);
					memmove(mBuffer, mBuffer + cnt, mFill - cnt);
					mFill -= cnt;

					assert(mFill >= 0 && mFill < 256);
				}
			}
			else
			{
				sprintf_s(line, 1024, "0x%02x, 0x%02x, ", 0x00 + (cnt - 1), (unsigned char)mBuffer[0]);
				memmove(mBuffer, mBuffer + cnt, mFill - cnt);
				mFill -= cnt;

				assert(mFill >= 0 && mFill < 256);
			}

			if (mFill == 0)
				strcat_s(line, 1024, "0x00, ");

			return true;
		}
		else
		{
			int	cnt = 1;
			int rep = 1;
			while (cnt < 64 && cnt < mFill && rep < 3)
			{
				if (mBuffer[cnt] == mBuffer[cnt - 1])
					rep++;
				else
					rep = 1;
				cnt++;
			}
			if (cnt < mFill && rep >= 3)
				cnt -= rep;

			sprintf_s(line, 1024, "0x%02x, ", 0x40 + (cnt - 1));

			for (int i = 0; i < cnt; i++)
			{
				char	buffer[16];
				sprintf_s(buffer, 16, "0x%02x, ", (unsigned char)mBuffer[i]);
				strcat_s(line, 1024, buffer);
			}

			memmove(mBuffer, mBuffer + cnt, mFill - cnt);
			mFill -= cnt;

			assert(mFill >= 0 && mFill < 256);

			if (mFill == 0)
				strcat_s(line, 1024, "0x00, ");

			return true;
		}
	}

	return false;
}

bool SourceFile::ReadLine(char* line)
{
	if (mFile)
	{
		switch (mMode)
		{
		case SFM_TEXT:
			if (fgets(line, 1024, mFile))
				return true;
			break;
		case SFM_BINARY:
			if (mLimit)
			{
				mLimit--;

				int c = fgetc(mFile);
				if (c >= 0)
				{
					sprintf_s(line, 1024, "0x%02x, ", c);
					return true;
				}
			}
			break;
		case SFM_BINARY_RLE:
			if (ReadLineRLE(line))
				return true;
			break;
		}

		fclose(mFile);
		mFile = nullptr;
	}

	return false;
}

void SourceFile::Limit(int skip, int limit)
{
	mLimit = limit;
	if (mFile)
		fseek(mFile, skip, SEEK_SET);
}

SourceFile::SourceFile(void) 
	: mFile(nullptr), mFileName{ 0 }, mStack(nullptr)
{

}

SourceFile::~SourceFile(void)
{
	if (mFile)
	{
		fclose(mFile);
		mFile = nullptr;
	}
}

bool SourceFile::Open(const char* name, const char* path, SourceFileMode mode)
{
	char	fname[220];

	if (strlen(name) + strlen(path) > 200)
		return false;

	strcpy_s(fname, path);
	int	n = strlen(fname);

	if (n > 0 && fname[n - 1] != '/')
	{
		fname[n++] = '/';
		fname[n] = 0;
	}

	strcat_s(fname + n, sizeof(fname) - n, name);

	if (!fopen_s(&mFile, fname, "rb"))
	{
		_fullpath(mFileName, fname, sizeof(mFileName));
		char* p = mFileName;
		while (*p)
		{
			if (*p == '\\')
				*p = '/';
			p++;
		}
		mMode = mode;
		mLimit = 0x10000;
		mFill = 0;

		return true;
	}

	return false;
}

void SourceFile::Close(void)
{
	if (mFile)
	{
		fclose(mFile);
		mFile = nullptr;
	}
}

bool SourceFile::PushSource(void)
{
	SourceStack* stack = new SourceStack();
	stack->mUp = mStack;
	mStack = stack;
	stack->mFilePos = ftell(mFile);
	stack->mLocation = mLocation;
	return true;
}

bool SourceFile::PopSource(void)
{
	SourceStack* stack = mStack;
	if (stack)
	{
		mLocation = stack->mLocation;
		fseek(mFile, stack->mFilePos, SEEK_SET);
		mStack = mStack->mUp;
		return true;
	}
	else
		return false;
}

bool SourceFile::DropSource(void)
{
	SourceStack* stack = mStack;
	if (stack)
	{
		mStack = mStack->mUp;
		return true;
	}
	else
		return false;
}

bool Preprocessor::NextLine(void)
{
	int	s = 0;
	while (mSource->ReadLine(mLine + s))
	{
		mLocation.mLine++;

		s = strlen(mLine);
		while (s > 0 && (mLine[s - 1] == '\n' || mLine[s - 1] == '\r'))
			s--;
		if (s == 0 || mLine[s - 1] != '\\')
			return true;
		s--;
	}

	return false;
}

bool Preprocessor::EmbedData(const char* reason, const char* name, bool local, int skip, int limit, SourceFileMode mode)
{
	if (strlen(name) > 200)
	{
		mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Binary file path exceeds max path length");
		return false;
	}

	if (mSource)
		mSource->mLocation = mLocation;

	SourceFile* source = new SourceFile();

	bool	ok = false;

	if (source->Open(name, "", mode))
		ok = true;

	if (!ok && local && mSource)
	{
		char	lpath[220];
		strcpy_s(lpath, mSource->mFileName);
		int	i = strlen(lpath);
		while (i > 0 && lpath[i - 1] != '/')
			i--;
		lpath[i] = 0;

		if (source->Open(name, lpath, mode))
			ok = true;
	}

	SourcePath* p = mPaths;
	while (!ok && p)
	{
		if (source->Open(name, p->mPathName, mode))
			ok = true;
		else
			p = p->mNext;
	}

	if (ok)
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("%s \"%s\"\n", reason, source->mFileName);

		source->Limit(skip, limit);

		source->mUp = mSource;
		mSource = source;
		mLocation.mFileName = mSource->mFileName;
		mLocation.mLine = 0;
		mLine[0] = 0;

		return true;
	}
	else
	{
		delete source;
		return false;
	}
}

bool Preprocessor::OpenSource(const char * reason, const char* name, bool local)
{
	if (strlen(name) > 200) 
	{
		mErrors->Error(mLocation, EERR_FILE_NOT_FOUND, "Source file path exceeds max path length");
		return false;
	}

	if (mSource)
		mSource->mLocation = mLocation;

	SourceFile	*	source = new SourceFile();

	bool	ok = false;

	if (source->Open(name, ""))
		ok = true;

	if (!ok && local && mSource)
	{
		char	lpath[220];
		strcpy_s(lpath, mSource->mFileName);
		int	i = strlen(lpath);
		while (i > 0 && lpath[i - 1] != '/')
			i--;
		lpath[i] = 0;

		if (source->Open(name, lpath))
			ok = true;
	}

	SourcePath* p = mPaths;
	while (!ok && p)
	{
		if (source->Open(name, p->mPathName))
			ok = true;
		else
			p = p->mNext;
	}
	
	if (ok)
	{
		if (mCompilerOptions & COPT_VERBOSE)
			printf("%s \"%s\"\n", reason, source->mFileName);

		source->mUp = mSource;
		mSource = source;
		mLocation.mFileName = mSource->mFileName;
		mLocation.mLine = 0;
		mLine[0] = 0;

		return true;
	}
	else
		return false;
}

bool Preprocessor::CloseSource(void)
{
	if (mSource)
	{
		mSource = mSource->mUp;
		if (mSource)
		{
			mLocation = mSource->mLocation;
			mLine[0] = 0;
			return true;
		}
	}
	
	return false;
}

bool Preprocessor::PushSource(void)
{
	mSource->mLocation = mLocation;
	return mSource->PushSource();
}

bool Preprocessor::PopSource(void)
{
	if (mSource->PopSource())
	{
		mLocation = mSource->mLocation;
		return true;
	}
	else
		return false;

}

bool Preprocessor::DropSource(void)
{
	return mSource->DropSource();
}

Preprocessor::Preprocessor(Errors* errors)
	: mSource(nullptr), mSourceList(nullptr), mPaths(nullptr), mErrors(errors), mCompilerOptions(COPT_DEFAULT)
{

}

Preprocessor::~Preprocessor(void)
{
}

void Preprocessor::AddPath(const char* path)
{
	SourcePath* sp = new SourcePath(path);
	sp->mNext = mPaths;
	mPaths = sp;
}

