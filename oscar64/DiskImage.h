#pragma once

#include "MachineTypes.h"

class DiskImage
{
public:
	DiskImage(const char * name);
	~DiskImage(void);

	bool WriteImage(const char* fname);

	bool OpenFile(const char* fname);
	void CloseFile(void);

	int WriteBytes(const uint8* data, ptrdiff_t size);
	bool WriteFile(const char* fname, bool compressed, int interleave);

protected:
	uint8		mSectors[41][21][256];

	void MarkBAMSector(int track, int sector);
	int AllocBAMSector(int track, int sector);
	int AllocBAMTrack(int track);

	uint8	*	mDirEntry;
	int			mTrack, mSector, mBytes, mInterleave;

};
