#include "Ident.h"
#include "MachineTypes.h"
#include <string.h>

Ident::~Ident()
{
	delete[] mString;
}

unsigned int IHash(const char* str)
{
	unsigned int	hash = 32324124;
	int		i = 0;
	while (str[i])
	{
		hash = hash * 123211 + str[i];
		i++;
	}

	return hash;
}

Ident::Ident(const char* str, unsigned int hash)
{
	int	ssize = strlen(str);
	mString = new char[ssize + 1];
	strcpy_s(mString, ssize + 1, str);

	mHash = hash;
}

Ident	*	UniqueIdents[0x10000];

const Ident* Ident::Unique(const char* str)
{
	unsigned int hash = IHash(str);
	int i = hash & 0xffff;
	while (UniqueIdents[i])
	{
		if (!strcmp(UniqueIdents[i]->mString, str))
			return UniqueIdents[i];
		i = (i + 1) & 0xffff;
	}
	return UniqueIdents[i] = new Ident(str, hash);
}


const Ident* Ident::PreMangle(const char* str) const
{
	char	buffer[200];
	strcpy_s(buffer, str);
	strcat_s(buffer, mString);
	return Unique(buffer);
}

const Ident* Ident::Mangle(const char* str) const
{
	char	buffer[200];
	strcpy_s(buffer, mString);
	strcat_s(buffer, str);
	return Unique(buffer);
}

IdentDict::IdentDict(void)
{
	mHashSize = 0;
	mHashFill = 0;
	mHash = nullptr;
}

IdentDict::~IdentDict(void)
{
	delete[] mHash;
}

void IdentDict::Insert(const Ident* ident, const char* str)
{
	int	s = strlen(str);
	char* nstr = new char[s + 1];
	strcpy_s(nstr, s + 1, str);
	InsertCopy(ident, nstr);
}

void IdentDict::InsertCopy(const Ident* ident, char* str)
{
	if (!mHash)
	{
		mHashSize = 16;
		mHashFill = 0;
		mHash = new Entry[mHashSize];
		for (int i = 0; i < mHashSize; i++)
		{
			mHash[i].mString = nullptr;
			mHash[i].mIdent = nullptr;
		}
	}

	int		hm = mHashSize - 1;
	int		hi = ident->mHash & hm;

	while (mHash[hi].mIdent)
	{
		if (ident == mHash[hi].mIdent)
		{
			mHash[hi].mString = str;
			return;
		}

		hi = (hi + 1) & hm;
	}

	mHash[hi].mIdent = ident;
	mHash[hi].mString = str;
	mHashFill++;

	if (2 * mHashFill >= mHashSize)
	{
		int		size = mHashSize;
		Entry* entries = mHash;
		mHashSize *= 2;
		mHashFill = 0;
		mHash = new Entry[mHashSize];
		for (int i = 0; i < mHashSize; i++)
		{
			mHash[i].mString = nullptr;
			mHash[i].mIdent = nullptr;
		}
		for (int i = 0; i < size; i++)
		{
			if (entries[i].mIdent)
				InsertCopy(entries[i].mIdent, entries[i].mString);
		}
		delete[] entries;
	}
}

const char * IdentDict::Lookup(const Ident* ident)
{
	if (mHashSize > 0)
	{
		int		hm = mHashSize - 1;
		int		hi = ident->mHash & hm;

		while (mHash[hi].mIdent)
		{
			if (ident == mHash[hi].mIdent)
				return mHash[hi].mString;
			hi = (hi + 1) & hm;
		}
	}

	return nullptr;
}

