#pragma once

#include <assert.h>
#include "MachineTypes.h"

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
	int			size, range;
	T		*	array;
	T			empty;

	void Grow(int to, bool clear)
	{
		T* a2;
		int i;

		if (clear) size = 0;

		if (to > range)
		{
			int trange = range * sizeof(T) < 65536 ? range * 2 : range + (range >> 2);

			if (trange < 4)
				trange = 4;

			if (to > trange)
				range = to;
			else
				range = trange;

			a2 = new T[range];
			if (to > size)
				for (i = 0; i < size; i++) a2[i] = array[i];
			else
				for (i = 0; i < to; i++) a2[i] = array[i];

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
		range = 0;
		array = nullptr;
	}

	GrowingArray(const GrowingArray& a)
		: empty(a.empty)
	{
		size = a.size;
		range = a.range;
		array = new T[range];

		int lsize = size;
		const T* sap = a.array;
		T* dap = array;
		for (int i = 0; i < lsize; i++) dap[i] = sap[i];
	}

	GrowingArray & operator=(const GrowingArray& a)
	{
		if (a.size != size)
			Grow(a.size, true);

		int lsize = size;
		const T* sap = a.array;
		T* dap = array;
		for (int i = 0; i < lsize; i++) dap[i] = sap[i];

		return *this;
	}

	~GrowingArray(void)
	{
		delete[] array;
	}

	void shrink(void)
	{
		size = 0;
		range = 0;
		delete[] array;
		array = nullptr;
	}

	__forceinline T& operator[](int n)
	{
		assert(n >= 0);
		if (n >= size) Grow(n + 1, false);
		return array[n];
	}

	__forceinline const T & operator[](int n) const
	{
		assert(n >= 0);
		if (n >= size) return empty;
		else return array[n];
	}

	const T getAt(int n) const
	{
		if (n >= size) return empty;
		else return array[n];
	}

	void destroyAt(int n)
	{
		if (n < size)
			array[n] = empty;
	}
	
	void Push(T t)
	{
		(*this)[size] = t;
	}

	T Pop(void)
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

	void Remove(int at)
	{
		while (at + 1 < size)
		{
			array[at] = array[at + 1];
			at++;
		}
		Grow(at, false);
	}

	void Remove(int at, int n)
	{
		while (at + n < size)
		{
			array[at] = array[at + n];
			at++;
		}
		Grow(at, false);
	}

	int RemoveAll(const T & t)
	{
		int j = 0, i = 0;
		while (i < size)
		{
			if (array[i] != t)
			{
				if (i != j)
					array[j] = array[i];
				j++;
			}
			i++;
		}

		Grow(j, false);

		return i - j;
	}


	T Top(void) const
	{
		return array[size - 1];
	}

	bool IsEmpty(void) const { return size == 0; }

	__forceinline int Size(void) const { return size; }

	T Last() const
	{
		assert(size > 0);
		return array[size - 1];
	}

	void SetSize(int size, bool clear = false)
	{
		Grow(size, clear);
	}

	void Reserve(int to)
	{
		if (to > range)
		{
			range = to;

			T* a2 = new T[range];
			if (to > size)
				for (int i = 0; i < size; i++) a2[i] = array[i];
			else
				for (int i = 0; i < to; i++) a2[i] = array[i];

			delete[] array;
			array = a2;

			for (int i = size; i < range; i++) array[i] = empty;
		}
	}

	void Clear(void)
	{
		Grow(size, true);
	}

	int IndexOf(const T& t) const
	{
		for (int i = 0; i < size; i++)
			if (array[i] == t)
				return i;
		return -1;
	}

	bool Contains(const T& t) const
	{
		for (int i = 0; i < size; i++)
			if (array[i] == t)
				return true;
		return false;
	}
};

template <class T>
class ExpandingArray
{
public:
protected:
	int			size, range;
	T		*	array;

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
			if (to > size)
				for (i = 0; i < size; i++) a2[i] = array[i];
			else
				for (i = 0; i < to; i++) a2[i] = array[i];

			delete[] array;
			array = a2;
		}

		for (int i = size; i < to; i++) array[i] = T{};

		size = to;
	}

	template<typename F>
	void Partition(const F & f, int l, int r)
	{
		if (r > l + 1)
		{
			int pi = l;
			T	p(array[pi]);

			for (int i = l + 1; i < r; i++)
			{
				if (f(array[i], p))
				{
					array[pi++] = array[i];
					array[i] = array[pi];
				}
			}
			array[pi] = p;

			Partition(f, l, pi);
			Partition(f, pi + 1, r);
		}
	}
public:
	ExpandingArray(void)
	{
		size = 0;
		range = 4;
		array = new T[range];
	}

	ExpandingArray(const ExpandingArray& a)
	{
		size = a.size;
		range = a.range;
		array = new T[range];

		int lsize = size;
		const T* sap = a.array;
		T* dap = array;
		for (int i = 0; i < lsize; i++) dap[i] = sap[i];
	}

	ExpandingArray& operator=(const ExpandingArray& a)
	{
		if (a.size != size)
			Grow(a.size, true);

		int lsize = size;
		const T* sap = a.array;
		T* dap = array;
		for (int i = 0; i < lsize; i++) dap[i] = sap[i];

		return *this;
	}

	~ExpandingArray(void)
	{
		delete[] array;
	}

	void Push(const T & t)
	{
		int s = size;
		Grow(size + 1, false);
		array[s] = t;
	}

	T Pop(void)
	{
		assert(size > 0);
		return array[--size];
	}

	void Insert(int at, T t)
	{
		Grow(size + 1, false);
		assert(at >= 0 && at < size);
		int	j = size - 1;
		while (j > at)
		{
			array[j] = array[j - 1];
			j--;
		}
		array[at] = t;
	}

	void Remove(int at)
	{
		assert(at >= 0 && at < size);
		while (at + 1 < size)
		{
			array[at] = array[at + 1];
			at++;
		}
		Grow(at, false);
	}

	void Remove(int at, int n)
	{
		assert(at >= 0 && at + n <= size);
		while (at + n < size)
		{
			array[at] = array[at + n];
			at++;
		}
		Grow(at, false);
	}

	int RemoveAll(const T& t)
	{
		int j = 0, i = 0;
		while (i < size)
		{
			if (array[i] != t)
			{
				if (i != j)
					array[j] = array[i];
				j++;
			}
			i++;
		}

		Grow(j, false);

		return i - j;
	}


	T Top(void) const
	{
		assert(size > 0);
		return array[size - 1];
	}

	bool IsEmpty(void) const { return size == 0; }

	int Size(void) const { return size; }

	T Last() const
	{
		assert(size > 0);
		return array[size - 1];
	}

	void SetSize(int size, bool clear = false)
	{
		assert(size >= 0);
		Grow(size, clear);
	}

	void Reserve(int to)
	{
		if (to > range)
		{
			range = to;

			T* a2 = new T[range];
			if (to > size)
				for (int i = 0; i < size; i++) a2[i] = array[i];
			else
				for (int i = 0; i < to; i++) a2[i] = array[i];

			delete[] array;
			array = a2;
		}
	}

	int IndexOf(const T& t) const
	{
		for (int i = 0; i < size; i++)
			if (array[i] == t)
				return i;
		return -1;
	}

	int IndexOrPush(const T& t)
	{
		for (int i = 0; i < size; i++)
			if (array[i] == t)
				return i;
		int s = size;
		Grow(size + 1, false);
		array[s] = t;
		return s;
	}

	bool Contains(const T& t) const
	{
		for (int i = 0; i < size; i++)
			if (array[i] == t)
				return true;
		return false;
	}

	void Fill(const T& t)
	{
		for (int i = 0; i < size; i++)
			array[i] = t;
	}

	void Clear(void)
	{
		for (int i = 0; i < size; i++)
			array[i] = T{};
	}

	template<typename F>
	void Sort(const F & f)
	{
		Partition(f, 0, size);
	}

	__forceinline T& operator[](int n)
	{
		assert(n >= 0 && n < size);
		return array[n];
	}

	__forceinline const T& operator[](int n) const
	{
		assert(n >= 0 && n < size);
		return array[n];
	}
};
