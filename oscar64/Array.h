#pragma once

#include <assert.h>

template <class T>
class DynamicArray
{
protected:
	int		size, range;
	T* array;

	void Grow(int by)
	{
		T* a2;
		int i;

		if (size + by > range)
		{
			range = (size + by) * 2;
			a2 = new T[range];
			for (i = 0; i < size; i++) a2[i] = array[i];
			delete[] array;
			array = a2;
		}

		size += by;

	}
public:
	DynamicArray(void)
	{
		size = 0;
		range = 4;
		array = new T[range];
	}

	~DynamicArray(void)
	{
		delete[] array;
	}

	T* GetAndReleaseArray(void)
	{
		T* a2 = array;

		size = 0;
		range = 4;
		array = new T[range];

		return a2;
	}

	int Size(void) { return size; }

	bool Insert(T t)
	{
		Grow(1);
		array[size - 1] = t;

		return true;
	}

	bool Insert(int n, T t)
	{
		int	m;

		if (n >= 0 && n <= size)
		{
			Grow(1);
			m = size - 1;
			while (m > n)
			{
				array[m] = array[m - 1];
				m--;
			}

			array[n] = t;
		}

		return true;
	}

	bool Lookup(int n, T& t)
	{
		if (n >= 0 && n < size)
		{
			t = array[n];
			return true;
		}
		else
			return false;
	}

	bool Replace(int n, T t, T& old)
	{
		if (n >= 0 && n < size)
		{
			old = array[n];
			array[n] = t;

			return true;
		}
		else
			return false;
	}

	bool Swap(int n, int m)
	{
		T t;

		if (n >= 0 && n < size && m >= 0 && m < size)
		{
			t = array[n]; array[n] = array[m]; array[m] = t;

			return true;
		}
		else
			return false;
	}

	bool Remove(int n)
	{
		if (n >= 0 && n < size)
		{
			while (n < size - 1)
			{
				array[n] = array[n + 1];
				n++;
			}
			size--;

			return true;
		}
		else
			return false;
	}

	int Find(T t)
	{
		int n;

		n = size - 1;
		while (n >= 0 && t != array[n])
			n--;

		return n;
	}
};

template <class T>
class GrowingArray
{
protected:
	int		size, range;
	T* array;
	T			empty;

	void Grow(int to, bool clear)
	{
		T* a2;
		int i;

		if (clear) size = 0;

		if (to > range)
		{
			if (to > range * 2)
				range = to;
			else
				range = range * 2;

			a2 = new T[range];
			for (i = 0; i < size; i++) a2[i] = array[i];
			delete[] array;
			array = a2;
		}

		for (i = size; i < to; i++) array[i] = empty;

		size = to;
	}

public:
	GrowingArray(T empty_)
		: empty(empty_)
	{
		size = 0;
		range = 4;
		array = new T[range];
	}

	GrowingArray(const GrowingArray& a)
		: empty(a.empty)
	{
		int i;
		size = a.size;
		range = a.range;
		array = new T[range];
		for (i = 0; i < size; i++) array[i] = a.array[i];
	}

	GrowingArray & operator=(const GrowingArray& a)
	{
		Grow(a.size, true);
		int i;
		for (i = 0; i < size; i++) array[i] = a.array[i];
		return *this;
	}

	~GrowingArray(void)
	{
		delete[] array;
	}

	__forceinline T& operator[](int n)
	{
		assert(n >= 0);
		if (n >= size) Grow(n + 1, false);
		return array[n];
	}

	__forceinline T operator[](int n) const
	{
		assert(n >= 0);
		if (n >= size) return empty;
		else return array[n];
	}

	__forceinline void Push(T t)
	{
		(*this)[size] = t;
	}

	__forceinline T Pop(void)
	{
		assert(size > 0);
		return array[--size];
	}

	void Insert(int at, T t)
	{
		Grow(size + 1, false);
		int	j = size - 1;
		while (j > at)
		{
			array[j] = array[j - 1];
			j--;
		}
		array[at] = t;
	}

	__forceinline T Top(void) const
	{
		return array[size - 1];
	}

	__forceinline bool IsEmpty(void) const { return size == 0; }

	__forceinline int Size(void) const { return size; }

	__forceinline void SetSize(int size, bool clear = false)
	{
		Grow(size, clear);
	}

	__forceinline void Clear(void)
	{
		Grow(size, true);
	}
};
