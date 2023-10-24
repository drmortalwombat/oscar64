#ifndef OPP_STATIC_VECTOR_H
#define OPP_STATIC_VECTOR_H

#include <new>
#include <stdlib.h>
#include <opp/utility.h>

namespace opp {

template <class T, int N>
class static_vector
{
protected:
	char	_space[N * sizeof(T)];
	enum { m = N }	_size;
public:
	typedef T 	element_type;

	static_vector(void) : _size(0) {}

	static_vector(size_t n) : _size(n) 
	{
		T *	data = (T*)_space;
		for(size_t i=0; i<n; i++)
			new (data + i) T;
	}

	static_vector(const static_vector & v)
		: _size(v._size) 
	{
		T *	data = (T*)_space, * vdata = (T*)(v._space);
		for(size_t i=0; i<_size; i++)
			new (data + i)T(vdata[i]);
	}

	~static_vector(void)
	{
		T *	data = (T*)_space;
		for(size_t i=0; i<_size; i++)
			data[i].~T();
	}

	static_vector & operator=(const static_vector & v)
	{
		if (this != &v)
		{
			T *	data = (T*)_space, * vdata = (T*)(v._space);
			for(size_t i=0; i<_size; i++)
				data[i].~T();
			_size = v._size; 
			for(size_t i=0; i<_size; i++)
				new (data + i)T(vdata[i]);
		}
		return *this;
	}

	size_t size(void) const
	{
		return _size;
	}

	size_t max_size(void) const
	{
		return N;
	}

	bool empty(void) const
	{
		return _size == 0;
	}

	size_t capacity(void) const
	{
		return N;
	}

	void resize(size_t n);

	T & at(size_t at)
	{
		return ((T*)_space)[at];
	}

	const T & at(size_t at) const
	{
		return ((T*)_space)[at];
	}

	T & operator[](size_t at)
	{
		return ((T*)_space)[at];
	}

	const T & operator[](size_t at) const
	{
		return ((T*)_space)[at];
	}

	T * begin(void)
	{
		return (T*)_space;
	}

	const T * begin(void) const
	{
		return (T*)_space;
	}

	const T * cbegin(void) const
	{
		return (T*)_space;
	}

	T * end(void)
	{
		return (T*)_space + _size;
	}

	const T * end(void) const
	{
		return (T*)_space + _size;
	}

	const T * cend(void) const
	{
		return (T*)_space + _size;
	}

	T & front(void)
	{
		return ((T*)_space)[0];
	}

	const T & front(void) const
	{
		return ((T*)_space)[0];
	}

	T & back(void)
	{
		return ((T*)_space)[_size - 1];
	}

	const T & back(void) const
	{
		return ((T*)_space)[_size - 1];
	}

	T * data(void)
	{
		return (T*)_space;
	}

	const T * at(void) const
	{
		return (T*)_space;
	}

	void push_back(const T & t);

	void push_back(T && t);

	void pop_back(void)
	{
		_size--;
		((T*)_space)[_size].~T();
	}

	void insert(size_t at, const T & t);

	void erase(size_t at, size_t n = 1);

	T * insert(T * at, const T & t);

	template <typename ...P>
	void emplace_back(const P&... p);
};



template <class T, int N>
void static_vector<T, N>::resize(size_t n)
{
	T *	data = (T*)_space;
	if (n < _size)
	{
		for(size_t i=n; i<_size; i++)
			data[i].~T();			
		_size = n;
	}
	else
	{
		for(size_t i=_size; i<n; i++)
			new(data + i)T;
		_size = n;
	}
}

template <class T, int N>
void static_vector<T, N>::push_back(const T & t)
{
	new ((T*)_space + _size++)T(t);
}

template <class T, int N>
void static_vector<T, N>::push_back(T && t)
{
	new ((T*)_space + _size++)T(t);
}

template <class T, int N>
template <typename ...P>
void static_vector<T, N>::emplace_back(const P&... p)
{
	new ((T*)_space + _size++)T(p...);
}

template <class T, int N>
void static_vector<T, N>::insert(size_t at, const T & t)
{
	T *	data = (T*)_space;
	new (data + _size)T;
	for(size_t i=_size; i>at; i--)
		data[i] = move(data[i - 1]);
	data[at] = t;
	_size++;
}

template <class T, int N>
void static_vector<T, N>::erase(size_t at, size_t n)
{
	T *	data = (T*)_space;
	_size -= n;
	for(size_t i=at; i<_size; i++)
		data[i] = move(data[i + n]);
	for(size_t i=0; i<n; i++)
		data[_size + i].~T();
}

template <class T, int N>
T * static_vector<T, N>::insert(T * at, const T & t)
{
	T *	data = (T*)_space;
	T * dp = data + _size;
	new (dp)T;
	while (dp != at)
	{
		dp--;
		dp[1] = move(dp[0]);
	}
	dp[0] = t;
	_size++;
	return dp + 1;
}

}
#endif

