#pragma once

#include "MachineTypes.h"

class Errors;

class DiskImage
{
public:
	enum class Format
	{
		D64,
		D81
	};

	DiskImage(const char * name, Errors* errors, Format format = Format::D64);
	~DiskImage(void);

	bool WriteImage(const char* fname);

	bool OpenFile(const char* fname);
	void CloseFile(void);

	void WriteBytes(const uint8* data, ptrdiff_t size);
	bool WriteFile(const char* fname, bool compressed, int interleave);

protected:
	uint8		mSectors[81][40][256];

	void MarkBAMSector(int track, int sector);
	int AllocBAMSector(int track, int sector);
	int AllocBAMTrack(int track);
	bool CapacityExceeded(void);
	uint8* BAMEntry(int track);
	int SectorsOnTrack(int track) const;

	Errors*		mErrors;
	uint8	*	mDirEntry;
	int			mTrack, mSector, mBytes, mInterleave;
	int			mTracks, mDirectoryTrack, mFirstDirectorySector;

};
