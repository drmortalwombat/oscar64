#include "DiskImage.h"

static char SectorsPerTrack[] = {
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

DiskImage::DiskImage(const char* fname)
{
	for (int i = 0; i < 41; i++)
		for (int j = 0; j < 21; j++)
			memset(mSectors[i][j], 0, 256);

	// init bam

	uint8* bam = mSectors[18][0];

	bam[0] = 18; bam[1] = 1; bam[2] = 0x41; bam[3] = 0;
	for (int i = 1; i < 36; i++)
	{
		uint8* dp = bam + 4 * i;
		dp[0] = SectorsPerTrack[i];
		unsigned k = (1 << SectorsPerTrack[i]) - 1;
		dp[3] = (k >> 16) & 255;
		dp[2] = (k >> 8) & 255;
		dp[1] =  k & 255;
	}

	for (int i = 0x90; i < 0xab; i++)
		bam[i] = 0xa0;

	char	dname[200];
	int		i = strlen(fname);

	while (i > 0 && fname[i - 1] != '/' && fname[i - 1] != '\\')
		i--;

	int	j = 0;
	while (j < 16 && fname[i + j] && fname[i + j] != '.')
	{
		bam[0x90 + j] = A2P(fname[i + j]);
		j++;
	}

	// DIsk ID
	bam[0xa2] = 'C';
	bam[0xa3] = 'N';

	// DOS TYPE
	bam[0xa5] = '2';
	bam[0xa6] = 'A';

	MarkBAMSector(18, 0);

	uint8* dir = mSectors[18][1];

	MarkBAMSector(18, 1);
	
	dir[1] = 0xff;

}

void DiskImage::MarkBAMSector(int track, int sector)
{
	uint8* bam = mSectors[18][0];

	uint8* dp = bam + 4 * track;

	if (dp[1 + (sector >> 3)] & (1 << (sector & 7)))
	{
		dp[1 + (sector >> 3)] &= ~(1 << (sector & 7));
		dp[0]--;
	}
}

int DiskImage::AllocBAMSector(int track, int sector)
{
	uint8* bam = mSectors[18][0];

	uint8* dp = bam + 4 * track;

	if (dp[0] > 0)
	{
		sector += 4;
		if (sector >= SectorsPerTrack[track])
			sector -= SectorsPerTrack[track];

		while (!(dp[1 + (sector >> 3)] & (1 << (sector & 7))))
		{
			sector++;
			if (sector >= SectorsPerTrack[track])
				sector -= SectorsPerTrack[track];
		}

		MarkBAMSector(track, sector);

		return sector;
	}
	else
		return -1;
}

int DiskImage::AllocBAMTrack(int track)
{
	uint8* bam = mSectors[18][0];

	if (track < 18)
	{
		while (track > 0 && bam[4 * track] == 0)
			track--;
		if (track != 0)
			return track;
		else
			track = 19;
	}

	while (track < 36 && bam[4 * track] == 0)
		track++;

	return track;
}

DiskImage::~DiskImage(void)
{

}

bool DiskImage::WriteImage(const char* fname)
{
	FILE* file;
	fopen_s(&file, fname, "wb");
	if (file)
	{
		for (int i = 1; i < 36; i++)
		{
			for(int j=0; j<SectorsPerTrack[i]; j++)
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
	int	si = 1;
	int	di = 0;

	for(;;)
	{
		mDirEntry = mSectors[18][si] + di;

		if (mDirEntry[2])
		{
			di += 32;
			if (di == 256)
			{
				di = 0;
				if (mSectors[18][si][0])
					si = mSectors[18][si][1];
				else
				{
					int ni = AllocBAMSector(18, si);
					mSectors[18][si][0] = 18;
					mSectors[18][si][1] = ni;
					si = ni;
					mSectors[18][si][1] = 0xff;
				}
			}
		}
		else
		{
			mTrack = AllocBAMTrack(17);
			mSector = AllocBAMSector(mTrack, 0);

			mDirEntry[2] = 0x82;
			mDirEntry[3] = mTrack;
			mDirEntry[4] = mSector;
			mBytes = 2;

			for (int i = 0; i < 16; i++)
				mDirEntry[5 + i] = 0xa0;

			int i = 0;
			while (fname[i])
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


bool DiskImage::WriteFile(const char* fname, bool compressed)
{
	FILE* file;
	fopen_s(&file, fname, "rb");
	if (file)
	{
		char	dname[200];
		int		i = strlen(fname);

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
			uint8	buffer[65536], cbuffer[65536];
			int		size = fread(buffer, 1, 65536, file);
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
		}

		fclose(file);
		return true;
	}
	else
		return false;
}

int DiskImage::WriteBytes(const uint8* data, int size)
{
	uint8* dp = mSectors[mTrack][mSector];
	for (int i = 0; i < size; i++)
	{
		if (mBytes >= 256)
		{
			mSector = AllocBAMSector(mTrack, mSector);
			if (mSector < 0)
			{
				mTrack = AllocBAMTrack(mTrack);
				mSector = AllocBAMSector(mTrack, 0);
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
	return 0;
}
