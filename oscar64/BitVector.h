#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

class BitVector
{
protected:
	unsigned* bits;
	unsigned			mask;
	int				size, dwsize;
public:
	BitVector(void);
	BitVector(const BitVector& v);
	BitVector(int size, bool set = false);
	BitVector(int size, unsigned char * data);
	BitVector& operator=(const BitVector& v);

	~BitVector(void);

	BitVector operator+(const BitVector v) const;
	BitVector operator*(const int n) const;
	BitVector operator|(const BitVector v) const;
	BitVector operator&(const BitVector v) const;
	BitVector operator~(void) const;

	void Set(int n, bool b);
	bool Get(int n) const;
	int Size(void) const;
	int DWSize(void) const;

	void Copy(unsigned* bits) const;
};

inline BitVector::BitVector(void)
{
	size = 0;
	dwsize = 0;
	bits = nullptr;
	mask = 0;
}

inline BitVector::BitVector(const BitVector& v)
{
	int i;

	size = v.size;
	dwsize = v.dwsize;
	mask = v.mask;

	if (dwsize)
		bits = new unsigned[dwsize];
	else
		bits = nullptr;

	for (i = 0; i < dwsize; i++) bits[i] = v.bits[i];
}

inline BitVector::BitVector(int size, unsigned char * data)
{
	this->size = size;
	dwsize = ((size + 31) >> 5);
	if ((size & 31) == 0)
		mask = 0xffffffff;
	else
		mask = (1 << (size & 31)) - 1;

	if (dwsize)
		bits = new unsigned[dwsize];
	else
		bits = nullptr;

	if (size > 0)
	{
		memcpy(bits, data, (size + 7) / 8);
	}
}

inline BitVector::BitVector(int size, bool set)
{
	int i;

	this->size = size;
	dwsize = ((size + 31) >> 5);
	if ((size & 31) == 0)
		mask = 0xffffffff;
	else
		mask = (1 << (size & 31)) - 1;

	if (dwsize)
		bits = new unsigned[dwsize];
	else
		bits = NULL;

	if (size > 0)
	{
		if (set)
		{
			for (i = 0; i < dwsize - 1; i++) bits[i] = ~0;
			bits[dwsize - 1] = mask;
		}
		else
			for (i = 0; i < dwsize; i++) bits[i] = 0;
	}
}

inline BitVector& BitVector::operator=(const BitVector& v)
{
	int i;

	delete[] bits;

	size = v.size;
	dwsize = v.dwsize;
	mask = v.mask;

	if (dwsize)
		bits = new unsigned[dwsize];
	else
		bits = NULL;

	for (i = 0; i < dwsize; i++) bits[i] = v.bits[i];

	return *this;
}


inline BitVector::~BitVector(void)
{
	delete[] bits;
}

inline BitVector BitVector::operator+(const BitVector v)	const
{
	int i;
	int s;
	unsigned merge;

	BitVector vv(size + v.size);

	if (dwsize == 0)
	{
		for (i = 0; i < v.dwsize; i++)
			vv.bits[i] = v.bits[i];
	}
	else if (v.dwsize == 0)
	{
		for (i = 0; i < dwsize; i++)
			vv.bits[i] = bits[i];
	}
	else
	{
		s = size & 31;

		if (s == 0)
		{
			for (i = 0; i < dwsize; i++)
				vv.bits[i] = bits[i];
			for (i = 0; i < v.dwsize; i++)
				vv.bits[i + dwsize] = v.bits[i];
		}
		else
		{
			for (i = 0; i < dwsize - 1; i++)
				vv.bits[i] = bits[i];

			merge = bits[dwsize - 1] & mask;

			for (i = 0; i < v.dwsize; i++)
			{
				vv.bits[i + dwsize - 1] = merge | (v.bits[i] << s);
				merge = v.bits[i] >> (32 - s);
			}

			if (dwsize - 1 + v.dwsize < vv.dwsize)
				vv.bits[dwsize - 1 + v.dwsize] = merge;
		}
	}

	return vv;
}

inline BitVector BitVector::operator*(const int n) const
{
	int i, j;
	int s, dw;
	unsigned merge;

	BitVector vv(size * n);

	if (n > 0)
	{
		dw = 0;

		for (i = 0; i < dwsize - 1; i++)
			vv.bits[dw++] = bits[i];

		merge = bits[dwsize - 1] & mask;

		for (j = 1; j < n; j++)
		{
			s = (size * j) & 31;

			if (s == 0)
			{
				vv.bits[dw++] = merge;
				for (i = 0; i < dwsize - 1; i++)
					vv.bits[dw++] = bits[i];

				merge = bits[dwsize - 1] & mask;
			}
			else
			{
				for (i = 0; i < dwsize - 1; i++)
				{
					vv.bits[dw++] = merge | (bits[i] << s);
					merge = bits[i] >> (32 - s);
				}
				if (mask >> (32 - s))
				{
					vv.bits[dw++] = merge | (bits[i] << s);
					merge = (bits[i] & mask) >> (32 - s);
				}
				else
				{
					merge |= (bits[i] & mask) << s;
				}
			}
		}

		if (dw < vv.dwsize)
			vv.bits[dw++] = merge;

		assert(dw <= vv.dwsize);
	}

	return vv;
}

inline BitVector BitVector::operator|(const BitVector v)	const
{
	int i;

	if (dwsize < v.dwsize)
	{
		BitVector vv(v.size);

		for (i = 0; i < dwsize; i++) vv.bits[i] = bits[i] | v.bits[i];
		for (i = dwsize; i < v.dwsize; i++) vv.bits[i] = v.bits[i];

		return vv;
	}
	else
	{
		BitVector vv(size);

		for (i = 0; i < v.dwsize; i++) vv.bits[i] = bits[i] | v.bits[i];
		for (i = v.dwsize; i < dwsize; i++) vv.bits[i] = bits[i];

		return vv;
	}
}

inline BitVector BitVector::operator&(const BitVector v)	const
{
	int i;

	if (dwsize < v.dwsize)
	{
		BitVector vv(v.size);

		for (i = 0; i < dwsize; i++) vv.bits[i] = bits[i] & v.bits[i];

		return vv;
	}
	else
	{
		BitVector vv(size);

		for (i = 0; i < v.dwsize; i++) vv.bits[i] = bits[i] & v.bits[i];

		return vv;
	}
}

inline BitVector BitVector::operator~(void)	const
{
	int i;

	BitVector vv(size);

	for (i = 0; i < dwsize - 1; i++) vv.bits[i] = ~bits[i];
	vv.bits[dwsize - 1] = ~bits[dwsize - 1] & mask;

	return vv;
}

inline void BitVector::Set(int n, bool b)
{
	if (b)
		bits[n >> 5] |= (1 << (n & 31));
	else
		bits[n >> 5] &= ~(1 << (n & 31));
}

inline bool BitVector::Get(int n)	const
{
	return (bits[n >> 5] & (1 << (n & 31))) != 0;
}

inline int BitVector::Size(void) const
{
	return size;
}

inline int BitVector::DWSize(void) const
{
	return dwsize;
}

inline void BitVector::Copy(unsigned* bits) const
{
	int i;

	if (dwsize > 0 && (size & 31))
		this->bits[dwsize - 1] &= (1 << (size & 31)) - 1;

	for (i = 0; i < dwsize; i++)
	{
		bits[i] = this->bits[i];
	}
}
