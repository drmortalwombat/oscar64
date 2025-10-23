#include "NumberSet.h"

#define VALGRIND	0

NumberSet::NumberSet(void)
{
	size = 0;
	dwsize = 0;
	bits = 0;
}

NumberSet::NumberSet(int size, bool set)
{
	int i;

	this->size = size;
	dwsize = (size + 63) >> 6;

	bits = new uint64[dwsize];

	if (set)
	{
		for (i = 0; i < dwsize; i++)
			bits[i] = ~0ull;
	}
	else
	{
		for (i = 0; i < dwsize; i++)
			bits[i] = 0;
	}
}

NumberSet::NumberSet(const NumberSet& set)
{
	int i;

	this->size = set.size;
	this->dwsize = set.dwsize;
	this->bits = new uint64[dwsize];

	for (i = 0; i < dwsize; i++)
		bits[i] = set.bits[i];
}

NumberSet::~NumberSet(void)
{
	delete[] bits;
}

void NumberSet::Expand(int size, bool set)
{
	if (size > this->size)
	{
		int ndwsize = (size + 63) >> 6;
		if (dwsize != ndwsize)
		{
			uint64	*	nbits = new uint64[ndwsize];
			for (int i = 0; i < dwsize; i++)
				nbits[i] = bits[i];
			for (int i = dwsize; i < ndwsize; i++)
				nbits[i] = 0;
			delete[] bits;
			dwsize = ndwsize;
			bits = nbits;
		}
		this->size = size;
	}
}

void NumberSet::Reset(int size, bool set)
{
	int i;

	int ndwsize = (size + 63) >> 6;
	if (this->dwsize != ndwsize)
	{
		delete[] bits;
		dwsize = ndwsize;
		bits = new uint64[dwsize];
	}

	this->size = size;

	if (set)
	{
		for (i = 0; i < dwsize; i++)
			bits[i] = ~0ull;
	}
	else
	{
		for (i = 0; i < dwsize; i++)
			bits[i] = 0;
	}
}

void NumberSet::AddRange(int elem, int num)
{
	for (int i = 0; i < num; i++)
		*this += elem + i;
}

void NumberSet::SubRange(int elem, int num)
{
	for (int i = 0; i < num; i++)
		*this -= elem + i;
}

bool NumberSet::RangeClear(int elem, int num) const
{
	for (int i = 0; i < num; i++)
		if ((*this)[elem + i])
			return false;
	return true;
}

bool NumberSet::RangeFilled(int elem, int num) const
{
	for (int i = 0; i < num; i++)
		if (!(*this)[elem + i])
			return false;
	return true;
}

void NumberSet::Fill(void)
{
	int i;

	for (i = 0; i < dwsize; i++)
		bits[i] = ~0ull;
}

void NumberSet::OrNot(const NumberSet& set)
{
	int i;

	for (i = 0; i < dwsize; i++)
		bits[i] |= ~set.bits[i];
}

void NumberSet::Clear(void)
{
	int i;

	for (i = 0; i < dwsize; i++)
		bits[i] = 0;
}

NumberSet& NumberSet::operator=(const NumberSet& set)
{
	this->size = set.size;

	if (dwsize != set.dwsize)
	{
		delete[] bits;
		this->dwsize = set.dwsize;
		this->bits = new uint64[dwsize];
	}

	int	size = dwsize;
	const uint64* sbits = set.bits;
	uint64* dbits = bits;

	for (int i = 0; i < size; i++)
		dbits[i] = sbits[i];

	return *this;
}

NumberSet& NumberSet::operator&=(const NumberSet& set)
{
	assert(dwsize == set.dwsize);

	int	size = dwsize;
	const uint64* sbits = set.bits;
	uint64* dbits = bits;

	for (int i = 0; i < size; i++)
		dbits[i] &= sbits[i];

	return *this;
}

NumberSet& NumberSet::operator|=(const NumberSet& set)
{
	assert(dwsize >= set.dwsize);

	int	size = dwsize < set.dwsize ? dwsize : set.dwsize;
	const uint64* sbits = set.bits;
	uint64* dbits = bits;

	for (int i = 0; i < size; i++)
		dbits[i] |= sbits[i];

	return *this;
}

NumberSet& NumberSet::operator-=(const NumberSet& set)
{
	assert(dwsize == set.dwsize);

	int i;

	for (i = 0; i < dwsize; i++)
		bits[i] &= ~set.bits[i];

	return *this;
}

bool NumberSet::operator<=(const NumberSet& set) const
{
	assert(dwsize == set.dwsize);

	int i;

	for (i = 0; i < dwsize; i++)
		if (bits[i] & ~set.bits[i]) return false;

	return true;
}



FastNumberSet::FastNumberSet(void)
{
	num = 0;
	size = 0;
	asize = 0;
	buffer = nullptr;
}

FastNumberSet::FastNumberSet(int size, bool set)
{
	this->size = this->asize = size;
	buffer = new uint32[2 * size];
#if VALGRIND
	for (int i = 0; i < 2 * size; i++)
		buffer[i] = 0;
#endif
	if (set)
	{
		for (num = 0; num < size; num++)
		{
			buffer[num] = num;
			buffer[num + size] = num;
		}
	}
	else
		num = 0;
}

FastNumberSet::FastNumberSet(const FastNumberSet& set)
{
	int i;

	this->size = this->asize = set.size;
	this->num = set.num;
	buffer = new uint32[2 * size];
#if VALGRIND
	for (int i = 0; i < 2 * size; i++)
		buffer[i] = 0;
#endif

	for (i = 0; i < num; i++)
	{
		buffer[i] = set.buffer[i];
		buffer[size + buffer[i]] = i;
	}
}

void FastNumberSet::Reset(int size, bool set)
{
	if (size > this->asize)
	{
		delete[] buffer;
		buffer = new uint32[2 * size];
#if VALGRIND
		for (int i = 0; i < 2 * size; i++)
			buffer[i] = 0;
#endif

		this->asize = size;
	}

	this->size = size;

	if (set)
	{
		for (num = 0; num < size; num++)
		{
			buffer[num] = num;
			buffer[num + size] = num;
		}
	}
	else
		num = 0;
}


FastNumberSet::~FastNumberSet(void)
{
	delete[] buffer;
}

FastNumberSet& FastNumberSet::operator=(const FastNumberSet& set)
{
	if (set.size > this->asize)
	{
		delete[] buffer;
		buffer = new uint32[2 * set.size];

		this->asize = set.size;
	}

	this->size = set.size;

	for (num = 0; num < set.num; num++)
	{
		buffer[num] = set.buffer[num];
		buffer[buffer[num] + size] = num;
	}

	return *this;
}

void FastNumberSet::Clear(void)
{
	num = 0;
}

int FastNumberSet::Index(int elem) const
{
	uint32 dw = buffer[size + elem];

	if (dw < uint32(num) && buffer[dw] == elem)
		return dw;
	else
		return -1;
}
