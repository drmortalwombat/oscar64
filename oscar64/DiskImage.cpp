#include "DiskImage.h"
#include "Errors.h"

static const uint8 D64SectorsPerTrack[] = {
	0,

	21, 21, 21, 21,
	21, 21, 21, 21,
	21, 21, 21, 21,
	21, 21, 21, 21,
	21,

	19, 19, 19, 19,
	19, 19, 19,

	18, 18, 18, 18,
	18, 18,

	17, 17, 17, 17,
	17,

	0, 0, 0, 0, 0
};

static char A2P(char ch)
{
	if (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z')
		return (ch ^ 0x20) & 0xdf;
	else
		return ch;
}

DiskImage::DiskImage(const char* fname, Errors* errors, Format format)
	: mErrors(errors), mDirEntry(nullptr), mTrack(0), mSector(0), mBytes(0)
{
	mTracks = format == Format::D81 ? 80 : 35;
	mDirectoryTrack = format == Format::D81 ? 40 : 18;
	mFirstDirectorySector = format == Format::D81 ? 3 : 1;
	mInterleave = format == Format::D81 ? 1 : 10;

	for (int i = 0; i < 81; i++)
		for (int j = 0; j < 40; j++)
			memset(mSectors[i][j], 0, 256);

	ptrdiff_t i = strlen(fname);
	while (i > 0 && fname[i - 1] != '/' && fname[i - 1] != '\\')
		i--;

	if (format == Format::D81)
	{
		uint8* header = mSectors[40][0];
		header[0] = 40;
		header[1] = 3;
		header[2] = 'D';
		for (int j = 4; j <= 21; j++)
			header[j] = 0xa0;
		for (int j = 0; j < 16 && fname[i + j] && fname[i + j] != '.'; j++)
			header[4 + j] = A2P(fname[i + j]);
		header[22] = 'C';
		header[23] = 'N';
		header[24] = 0xa0;
		header[25] = '3';
		header[26] = 'D';
		header[27] = 0xa0;
		header[28] = 0xa0;

		uint8* bam1 = mSectors[40][1];
		uint8* bam2 = mSectors[40][2];
		bam1[0] = 40;
		bam1[1] = 2;
		bam2[0] = 0;
		bam2[1] = 0xff;
		bam1[2] = bam2[2] = 'D';
		bam1[3] = bam2[3] = 0xbb;
		bam1[4] = bam2[4] = 'C';
		bam1[5] = bam2[5] = 'N';
		bam1[6] = bam2[6] = 0xc0;

		for (int track = 1; track <= 80; track++)
		{
			uint8* dp = BAMEntry(track);
			dp[0] = 40;
			for (int j = 1; j < 6; j++)
				dp[j] = 0xff;
		}

		MarkBAMSector(40, 0);
		MarkBAMSector(40, 1);
		MarkBAMSector(40, 2);
		MarkBAMSector(40, 3);
	}
	else
	{
		uint8* bam = mSectors[18][0];

		bam[0] = 18; bam[1] = 1; bam[2] = 0x41; bam[3] = 0;
		for (int track = 1; track <= 35; track++)
		{
			uint8* dp = BAMEntry(track);
			dp[0] = D64SectorsPerTrack[track];
			unsigned k = (1 << D64SectorsPerTrack[track]) - 1;
			dp[3] = (k >> 16) & 255;
			dp[2] = (k >> 8) & 255;
			dp[1] = k & 255;
		}

		for (int j = 0x90; j < 0xab; j++)
			bam[j] = 0xa0;

		int j = 0;
		while (j < 16 && fname[i + j] && fname[i + j] != '.')
		{
			bam[0x90 + j] = A2P(fname[i + j]);
			j++;
		}

		// Disk ID
		bam[0xa2] = 'C';
		bam[0xa3] = 'N';

		// DOS type
		bam[0xa5] = '2';
		bam[0xa6] = 'A';

		MarkBAMSector(18, 0);
		MarkBAMSector(18, 1);
	}

	uint8* dir = mSectors[mDirectoryTrack][mFirstDirectorySector];
	dir[1] = 0xff;
}

int DiskImage::SectorsOnTrack(int track) const
{
	if (track <= 0 || track > mTracks)
		return 0;
	return mTracks == 80 ? 40 : D64SectorsPerTrack[track];
}

uint8* DiskImage::BAMEntry(int track)
{
	if (mTracks == 80)
	{
		int bamSector = track <= 40 ? 1 : 2;
		int bamTrack = (track - 1) % 40;
		return mSectors[40][bamSector] + 16 + 6 * bamTrack;
	}
	return mSectors[18][0] + 4 * track;
}

void DiskImage::MarkBAMSector(int track, int sector)
{
	uint8* dp = BAMEntry(track);

	if (dp[1 + (sector >> 3)] & (1 << (sector & 7)))
	{
		dp[1 + (sector >> 3)] &= ~(1 << (sector & 7));
		dp[0]--;
	}
}

int DiskImage::AllocBAMSector(int track, int sector)
{
	int sectors = SectorsOnTrack(track);

	if (sectors == 0)
		return -1;

	uint8* dp = BAMEntry(track);

	if (dp[0] > 0)
	{
		sector = (sector + mInterleave) % sectors;

		if (sector < 0)
			sector += sectors;

		while (!(dp[1 + (sector >> 3)] & (1 << (sector & 7))))
		{
			sector++;

			if (sector >= sectors)
				sector = 0;
		}

		MarkBAMSector(track, sector);

		return sector;
	}
	else
		return -1;
}

int DiskImage::AllocBAMTrack(int track)
{
	if (track < mDirectoryTrack)
	{
		while (track > 0 && BAMEntry(track)[0] == 0)
			track--;
		if (track != 0)
			return track;
		else
			track = mDirectoryTrack + 1;
	}

	if (track == mDirectoryTrack)
		track++;

	while (track <= mTracks && BAMEntry(track)[0] == 0)
		track++;
	return track <= mTracks ? track : -1;
}

DiskImage::~DiskImage(void)
{

}

bool DiskImage::CapacityExceeded(void)
{
	mErrors->Error(Location(), EERR_DISK_IMAGE_FULL, "Disk image capacity exceeded");
	return false;
}

bool DiskImage::WriteImage(const char* fname)
{
	FILE* file;
	fopen_s(&file, fname, "wb");
	if (file)
	{
		for (int i = 1; i <= mTracks; i++)
		{
			for (int j = 0; j < SectorsOnTrack(i); j++)
				fwrite(mSectors[i][j], 1, 256, file);
		}
		fclose(file);
		return true;
	}
	else
		return false;
}

bool DiskImage::OpenFile(const char* fname)
{
	int	si = mFirstDirectorySector;
	int	di = 0;

	for(;;)
	{
		mDirEntry = mSectors[mDirectoryTrack][si] + di;

		if (mDirEntry[2])
		{
			di += 32;
			if (di == 256)
			{
				di = 0;
				if (mSectors[mDirectoryTrack][si][0])
					si = mSectors[mDirectoryTrack][si][1];
				else
				{
					int ni = AllocBAMSector(mDirectoryTrack, si);
					if (ni < 0)
						return CapacityExceeded();
					mSectors[mDirectoryTrack][si][0] = mDirectoryTrack;
					mSectors[mDirectoryTrack][si][1] = ni;
					si = ni;
					mSectors[mDirectoryTrack][si][1] = 0xff;
				}
			}
		}
		else
		{
			int track = AllocBAMTrack(mDirectoryTrack - 1);
			int sector = AllocBAMSector(track, 0);
			if (sector < 0)
				return CapacityExceeded();

			mTrack = track;
			mSector = sector;

			mDirEntry[2] = 0x82;
			mDirEntry[3] = mTrack;
			mDirEntry[4] = mSector;
			mBytes = 2;

			for (int i = 0; i < 16; i++)
				mDirEntry[5 + i] = 0xa0;

			int i = 0;
			while (i < 16 && fname[i])
			{
				mDirEntry[5 + i] = A2P(fname[i]);
				i++;
			}

			mDirEntry[30] = 1;

			return true;
		}
	}

	return false;
}

void DiskImage::CloseFile(void)
{

}


bool DiskImage::WriteFile(const char* fname, bool compressed, int interleave)
{
	if (interleave >= 0)
		mInterleave = interleave;

	FILE* file;
	fopen_s(&file, fname, "rb");
	if (file)
	{
		char	dname[200];
		ptrdiff_t	i = strlen(fname);

		while (i > 0 && fname[i - 1] != '/' && fname[i - 1] != '\\')
			i--;

		int	j = 0;
		while (j < 16 && fname[i + j] && fname[i + j] != '.')
		{
			dname[j] = A2P(fname[i + j]);
			j++;
		}
		dname[j] = 0;

		if (OpenFile(dname))
		{
			uint8	* buffer = new uint8[65536], * cbuffer = new uint8[65536];
			ptrdiff_t	size = fread(buffer, 1, 65536, file);
			int		csize = 0;

			if (compressed)
			{
				int	pos = 0;
				while (pos < size)
				{
					int	pi = 0;
					while (pi < 127 && pos < size)
					{
						int	bi = pi, bj = 0;
						for (int i = 1; i < (pos < 255 ? pos : 255); i++)
						{
							int j = 0;
							while (j < 127 && pos + j < size && buffer[pos - i + j] == buffer[pos + j])
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
								cbuffer[csize++] = pi;
								for (int i = 0; i < pi; i++)
									cbuffer[csize++] = buffer[pos - pi + i];
								pi = 0;
							}

							cbuffer[csize++] = 128 + bj;
							cbuffer[csize++] = bi;
							pos += bj;
						}
						else
						{
							pos++;
							pi++;
						}
					}

					if (pi > 0)
					{
						cbuffer[csize++] = pi;
						for (int i = 0; i < pi; i++)
							cbuffer[csize++] = buffer[pos - pi + i];
					}
				}

				cbuffer[csize++] = 0;
				WriteBytes(cbuffer, csize);
			}
			else
				WriteBytes(buffer, size);
			CloseFile();

			delete[] buffer;
			delete[] cbuffer;
		}

		fclose(file);
		return true;
	}
	else
		return false;
}

void DiskImage::WriteBytes(const uint8* data, ptrdiff_t size)
{
	uint8* dp = mSectors[mTrack][mSector];
	for (ptrdiff_t i = 0; i < size; i++)
	{
		if (mBytes >= 256)
		{
			mSector = AllocBAMSector(mTrack, mSector);
			if (mSector < 0)
			{
				int track = AllocBAMTrack(mTrack);
				int sector = AllocBAMSector(track, 0);
				if (sector < 0)
				{
					CapacityExceeded();
					return;
				}

				mTrack = track;
				mSector = sector;
			}

			dp[0] = mTrack;
			dp[1] = mSector;

			mBytes = 2;
			if (!++mDirEntry[30])
				mDirEntry[31]++;

			dp = mSectors[mTrack][mSector];
		}

		dp[1] = mBytes;
		dp[mBytes] = data[i];
		mBytes++;
	}
}
