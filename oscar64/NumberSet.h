#pragma once

#include <assert.h>

#include "MachineTypes.h"

class NumberSet
{
protected:
	uint64				* bits;
	int					size, dwsize;
public:
	NumberSet(void);
	NumberSet(int size, bool set = false);
	NumberSet(const NumberSet& set);
	~NumberSet(void);

	void Reset(int size, bool set = false);
	void Expand(int size, bool set = false);

	NumberSet& operator+=(int elem);
	NumberSet& operator-=(int elem);

	bool operator[](int elem) const;

	NumberSet& operator=(const NumberSet& set);

	NumberSet& operator&=(const NumberSet& set);
	NumberSet& operator|=(const NumberSet& set);
	NumberSet& operator-=(const NumberSet& set);

	bool operator<=(const NumberSet& set) const;

	void OrNot(const NumberSet& set);

	void Clear(void);
	void Fill(void);
	void AddRange(int elem, int num);
	void SubRange(int elem, int num);
	bool RangeClear(int elem, int num) const;
	bool RangeFilled(int elem, int num) const;

	int Size(void) { return size; }
};

inline NumberSet& NumberSet::operator+=(int elem)
{
	assert(elem >= 0 && elem < size);
	bits[elem >> 6] |= (1ULL << (elem & 63));

	return *this;
}

inline NumberSet& NumberSet::operator-=(int elem)
{
	assert(elem >= 0 && elem < size);
	bits[elem >> 6] &= ~(1ULL << (elem & 63));

	return *this;
}

inline bool NumberSet::operator[](int elem) const
{
	assert(elem >= 0 && elem < size);
	return (bits[elem >> 6] & (1ULL << (elem & 63))) != 0;
}


class FastNumberSet
{
protected:
	uint32	*	buffer;
	int			size, num, asize;
public:
	FastNumberSet(void);
	FastNumberSet(int size, bool set = false);
	FastNumberSet(const FastNumberSet& set);
	~FastNumberSet(void);

	void Reset(int size, bool set = false);

	FastNumberSet& operator+=(int elem);
	FastNumberSet& operator-=(int elem);

	bool operator[](int elem) const;

	FastNumberSet& operator=(const FastNumberSet& set);

	bool Empty(void) const { return !num; }
	void Clear(void);

	int Num(void) const { return num; }
	int Element(int i) const;

	int Size(void) const { return size; }
	int Index(int elem) const;
};

inline bool FastNumberSet::operator[](int elem) const
{
	uint32 dw = buffer[size + elem];

	return (dw < uint32(num) && buffer[dw] == elem);
}

inline FastNumberSet& FastNumberSet::operator+=(int elem)
{
	assert(elem < size);

	uint32 dw = buffer[size + elem];

	if (dw >= uint32(num) || buffer[dw] != elem)
	{
		assert(num < size);

		buffer[num] = elem;
		buffer[size + elem] = num;
		num++;
	}

	return *this;
}

inline FastNumberSet& FastNumberSet::operator-=(int elem)
{
	uint32 dw = buffer[size + elem];

	if (dw < uint32(num) && buffer[dw] == elem)
	{
		num--;
		buffer[dw] = buffer[num];
		buffer[size + buffer[dw]] = dw;
	}

	return *this;
}

inline int FastNumberSet::Element(int i) const
{
	if (i < num)
		return buffer[i];
	else
		return -1;
}
