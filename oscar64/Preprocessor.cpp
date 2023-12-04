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

bool SourceFile::ReadLineLZO(char* line)
{
	if (mPos > 256)
	{
		memmove(mBuffer, mBuffer + mPos - 256, mFill + 256 - mPos);
		mFill -= mPos - 256;
		mPos = 256;
	}

	assert(mFill >= 0 && mFill - mPos < 384 && mPos <= mFill);

	int	c;
	while (mLimit && mFill < 384 && (c = ReadChar()) >= 0)
	{
		mLimit--;
		mBuffer[mFill++] = c;
	}

	if (mPos < mFill)
	{
		int	pi = 0;
		while (pi < 127 && mPos < mFill)
		{
			int	bi = pi, bj = 0;
			for (int i = 1; i < mPos; i++)
			{
				int j = 0;
				while (j < 127 && mPos + j < mFill && mBuffer[mPos - i + j] == mBuffer[mPos + j])
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
					sprintf_s(line, 1024, "0x%02x, ", pi);

					for (int i = 0; i < pi; i++)
					{
						char	buffer[16];
						sprintf_s(buffer, 16, "0x%02x, ", (unsigned char)mBuffer[mPos - pi + i]);

						strcat_s(line, 1024, buffer);
					}

					return true;
				}
				else
				{
					sprintf_s(line, 1024, "0x%02x, 0x%02x, ", 128 + bj, bi);
					mPos += bj;

					if (mFill == mPos)
						strcat_s(line, 1024, "0x00, ");

					return true;
				}
			}
			else
			{
				mPos++;
				pi++;
			}
		}

		sprintf_s(line, 1024, "0x%02x, ", pi);

		for (int i = 0; i < pi; i++)
		{
			char	buffer[16];
			sprintf_s(buffer, 16, "0x%02x, ", (unsigned char)mBuffer[mPos - pi + i]);

			strcat_s(line, 1024, buffer);
		}

		if (mFill == mPos)
			strcat_s(line, 1024, "0x00, ");

		return true;
	}

	return false;
}

bool SourceFile::ReadLineRLE(char* line)
{
	assert(mFill >= 0 && mFill < 256);

	int	c;
	while (mLimit && mFill < 256 && (c = ReadChar()) >= 0)
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

int SourceFile::ReadChar(void)
{
	if (mMemData)
	{
		if (mMemPos < mMemSize)
			return mMemData[mMemPos++];
		else
			return -1;
	}
	else
		return fgetc(mFile);
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

				int c = ReadChar();
				if (c >= 0)
				{
					sprintf_s(line, 1024, "0x%02x, ", c);
					return true;
				}
			}
			break;
		case SFM_BINARY_WORD:
			if (mLimit >= 2)
			{
				mLimit -= 2;

				int c = ReadChar();
				if (c >= 0)
				{
					int d = ReadChar();
					if (d >= 0)
						sprintf_s(line, 1024, "0x%04x, ", c + 256 * d);
					return true;
				}
			}
			break;

		case SFM_BINARY_RLE:
			if (ReadLineRLE(line))
				return true;
			break;
		case SFM_BINARY_LZO:
			if (ReadLineLZO(line))
				return true;
			break;
		}

		fclose(mFile);
		mFile = nullptr;
	}

	return false;
}

struct CTMHeader8
{
	uint8	mID[3];
	uint8	mVersion[1];
	uint8	mDispMode;
	uint8	mColorMethod;
	uint8	mFlags;
	uint8	mColors[7];
};

#pragma pack(push, 1)
struct SPDHeader5
{
	uint8	mID[3];
	uint8	mVersion[1];
	uint8	mFlags;
	uint16	mNumSprites, mNumTiles;
	uint8	mNumSpriteAnmis, mNumTileAnims;
	uint8	mTileWidth, mTileHeight;
	uint8	mColors[3];
	int16	mSpriteOverlayDist, mTileOverlayDist;
};
#pragma pack(pop)

void SourceFile::ReadSpritePad(SourceFileDecoder decoder)
{
	SPDHeader5	spdHeader;

	fread(&spdHeader, sizeof(SPDHeader5), 1, mFile);

	if (decoder == SFD_SPD_SPRITES)
	{
		mLimit = 64 * spdHeader.mNumSprites;
		return;
	}

	mLimit = 0;
}

void SourceFile::ReadCharPad(SourceFileDecoder decoder)
{
	CTMHeader8	ctmHeader;
	uint16		ctmMarker, numChars, numTiles;
	char		tileWidth, tileHeight;

	fread(&ctmHeader, sizeof(CTMHeader8), 1, mFile);
	fread(&ctmMarker, 2, 1, mFile);
	fread(&numChars, 2, 1, mFile);
	numChars++;

	if (decoder == SFD_CTM_CHARS)
	{
		mLimit = 8 * numChars;
		return;
	}

	// Skip chars
	fseek(mFile, 8 * numChars, SEEK_CUR);

	fread(&ctmMarker, 2, 1, mFile);

	if (decoder == SFD_CTM_CHAR_ATTRIB_1 || decoder == SFD_CTM_CHAR_ATTRIB_2)
	{
		mMemSize = numChars;
		mLimit = mMemSize;
		mMemPos = 0;
		mMemData = new uint8[mMemSize];
	}

	if (decoder == SFD_CTM_CHAR_ATTRIB_1)
	{
		for (int i = 0; i < mMemSize; i++)
		{
			uint8	fd;
			fread(&fd, 1, 1, mFile);
			mMemData[i] = fd << 4;
		}
	}
	else
	{
		// Skip material
		fseek(mFile, 1 * numChars, SEEK_CUR);
	}

	if (ctmHeader.mColorMethod == 2)
	{
		fread(&ctmMarker, 2, 1, mFile);

		if (decoder == SFD_CTM_CHAR_ATTRIB_1 || decoder == SFD_CTM_CHAR_ATTRIB_2)
		{
			if (ctmHeader.mDispMode == 4)
			{
				for (int i = 0; i < mMemSize; i++)
				{
					uint8	fd[3];
					fread(fd, 1, 3, mFile);
					if (decoder == SFD_CTM_CHAR_ATTRIB_1)
						mMemData[i] |= fd[0];
					else
						mMemData[i] = (fd[2] << 4) | fd[1];
				}
			}
			else
			{
				for (int i = 0; i < mMemSize; i++)
				{
					uint8	fd[3];
					fread(fd, 1, 1, mFile);
					mMemData[i] |= fd[0];
				}
			}


			return;
		}
		else
		{
			// Skip colors
			if (ctmHeader.mDispMode == 3)
				fseek(mFile, 2 * numChars, SEEK_CUR);
			else if (ctmHeader.mDispMode == 4)
				fseek(mFile, 3 * numChars, SEEK_CUR);
			else
				fseek(mFile, numChars, SEEK_CUR);
		}
	}
	else if (decoder == SFD_CTM_CHAR_ATTRIB_1)
	{
		return;
	}

	if (ctmHeader.mFlags & 1)
	{
		fread(&ctmMarker, 2, 1, mFile);

		fread(&numTiles, 2, 1, mFile);
		numTiles++;

		fread(&tileWidth, 1, 1, mFile);
		fread(&tileHeight, 1, 1, mFile);

		if (decoder == SFD_CTM_TILES_16)
		{
			mLimit = 2 * numTiles * tileWidth * tileHeight;
			return;
		}
		else if (decoder == SFD_CTM_TILES_8)
		{
			mMemSize = numTiles * tileWidth * tileHeight;
			mLimit = mMemSize;
			mMemPos = 0;
			mMemData = new uint8[mMemSize];
			for (int i = 0; i < mMemSize; i++)
			{
				int16	d;
				fread(&d, 2, 1, mFile);
				mMemData[i] = uint8(d);
			}
			return;
		}
		else
			fseek(mFile, 2 * numTiles * tileWidth * tileHeight, SEEK_CUR);

		if (ctmHeader.mColorMethod == 1)
		{
			fread(&ctmMarker, 2, 1, mFile);
			if (decoder == SFD_CTM_CHAR_ATTRIB_1 || decoder == SFD_CTM_CHAR_ATTRIB_2)
			{
				mMemSize = numTiles;
				mLimit = mMemSize;
				mMemPos = 0;
				mMemData = new uint8[mMemSize];

				if (ctmHeader.mDispMode == 4)
				{
					for (int i = 0; i < mMemSize; i++)
					{
						uint8	fd[3];
						fread(fd, 1, 3, mFile);
						if (decoder == SFD_CTM_CHAR_ATTRIB_1)
							mMemData[i] = fd[0];
						else
							mMemData[i] = (fd[2] << 4) | fd[1];
					}
				}
				else
				{
					for (int i = 0; i < mMemSize; i++)
					{
						uint8	fd[3];
						fread(fd, 1, 1, mFile);
						mMemData[i] = fd[0];
					}
				}

				return;
			}
			else
			{
				// Skip colors
				if (ctmHeader.mDispMode == 3)
					fseek(mFile, 2 * numTiles, SEEK_CUR);
				else if (ctmHeader.mDispMode == 4)
					fseek(mFile, 3 * numTiles, SEEK_CUR);
				else
					fseek(mFile, numTiles, SEEK_CUR);
			}
		}

		// Skip tags
		fread(&ctmMarker, 2, 1, mFile);
		fseek(mFile, 1 * numTiles, SEEK_CUR);

		// Skip tile names
		fread(&ctmMarker, 2, 1, mFile);
		for (int i = 0; i < numTiles; i++)
		{
			while (fgetc(mFile) > 0)
				;
		}
	}

	fread(&ctmMarker, 2, 1, mFile);

	int16	mapWidth, mapHeight;
	fread(&mapWidth, 2, 1, mFile);
	fread(&mapHeight, 2, 1, mFile);

	if (decoder == SFD_CTM_MAP_16)
	{
		mLimit = 2 * mapWidth * mapHeight;
		return;
	}
	else if (decoder == SFD_CTM_MAP_8)
	{
		mMemSize = mapWidth * mapHeight;
		mLimit = mMemSize;
		mMemPos = 0;
		mMemData = new uint8[mMemSize];
		for (int i = 0; i < mMemSize; i++)
		{
			int16	d;
			fread(&d, 2, 1, mFile);
			mMemData[i] = uint8(d);
		}
		return;
	}
	else
		fseek(mFile, 2 * mapWidth * mapHeight, SEEK_CUR);

	mLimit = 0;
}

void SourceFile::Limit(SourceFileDecoder decoder, int skip, int limit)
{
	switch (decoder)
	{
	case SFD_CTM_CHARS:
	case SFD_CTM_CHAR_ATTRIB_1:
	case SFD_CTM_CHAR_ATTRIB_2:
	case SFD_CTM_TILES_8:
	case SFD_CTM_TILES_16:
	case SFD_CTM_MAP_8:
	case SFD_CTM_MAP_16:
		ReadCharPad(decoder);
		break;

	case SFD_SPD_SPRITES:
		ReadSpritePad(decoder);
		break;

	default:
		mLimit = limit;
		if (mFile)
			fseek(mFile, skip, SEEK_SET);
	}
}

SourceFile::SourceFile(void) 
	: mFile(nullptr), mFileName{ 0 }, mStack(nullptr), mMemData(nullptr)
{

}

SourceFile::~SourceFile(void)
{
	if (mFile)
	{
		fclose(mFile);
		mFile = nullptr;
	}

	if (mMemData)
	{
		delete[] mMemData;
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
		strcpy_s(mLocationFileName, mFileName);
		mMode = mode;
		mLimit = 0x10000;
		mFill = 0;
		mPos = 0;

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

bool Preprocessor::EmbedData(const char* reason, const char* name, bool local, int skip, int limit, SourceFileMode mode, SourceFileDecoder decoder)
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

		source->Limit(decoder, skip, limit);

		source->mUp = mSource;
		mSource = source;
		mLocation.mFileName = mSource->mLocationFileName;
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
		mLocation.mFileName = mSource->mLocationFileName;
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

