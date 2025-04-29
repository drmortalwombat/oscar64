#ifndef OPP_VECTOR_H
#define OPP_VECTOR_H

#include <new>
#include <stdlib.h>
#include <opp/utility.h>

namespace opp {

template <class T>
class vector
{
protected:
	T	*	_data;
	size_t	_size, _capacity;
public:
	typedef T 	element_type;

	vector(void) : _data(nullptr), _size(0), _capacity(0) {}

	vector(size_t n) : _data((T*)malloc(n * sizeof(T))), _size(n), _capacity(n) 
	{
		for(size_t i=0; i<n; i++)
			new (_data + i) T();
	}

	vector(const vector & v)
		: _data((T*)malloc(v._size * sizeof(T))), _size(v._size), _capacity(v._size) 
	{
		for(size_t i=0; i<_size; i++)
			new (_data + i)T(v._data[i]);
	}

	vector(vector && v)
		: _data(v._data), _size(v._size), _capacity(v._capacity)
	{
		v._data = nullptr;
		v._size = 0;
		v._capacity = 0;
	}

	~vector(void)
	{
		for(size_t i=0; i<_size; i++)
			_data[i].~T();
		free(_data);
	}

	vector & operator=(const vector & v)
	{
		if (this != &v)
		{
			for(size_t i=0; i<_size; i++)
				_data[i].~T();
			free(_data);

			_data = (T*)malloc(v._size * sizeof(T));
			_size = v._size; 
			_capacity = v._size; 
			for(size_t i=0; i<_size; i++)
				new (_data + i)T(v._data[i]);		
		}
		return *this;
	}

	vector & operator=(vector && v)
	{
		if (this != &v)
		{
			swap(_data, v._data);
			swap(_size, v._size);
			swap(_capacity, v._capacity);
		}
		return *this;
	}

	int size(void) const
	{
		return _size;
	}

	int max_size(void) const
	{
		return 32767;
	}

	bool empty(void) const
	{
		return _size == 0;
	}

	int capacity(void) const
	{
		return _capacity;
	}

	void resize(size_t n);

	void reserve(size_t n);

	void shrink_to_fit(void);

	T & at(size_t at)
	{
		return _data[at];
	}

	const T & at(size_t at) const
	{
		return _data[at];
	}

	T & operator[](size_t at)
	{
		return _data[at];
	}

	const T & operator[](size_t at) const
	{
		return _data[at];
	}

	T * begin(void)
	{
		return _data;
	}

	const T * begin(void) const
	{
		return _data;
	}

	const T * cbegin(void) const
	{
		return _data;
	}

	T * end(void)
	{
		return _data + _size;
	}

	const T * end(void) const
	{
		return _data + _size;
	}

	const T * cend(void) const
	{
		return _data + _size;
	}

	T & front(void)
	{
		return _data[0];
	}

	const T & front(void) const
	{
		return _data[0];
	}

	T & back(void)
	{
		return _data[_size - 1];
	}

	const T & back(void) const
	{
		return _data[_size - 1];
	}

	T * data(void)
	{
		return _data;
	}

	const T * at(void) const
	{
		return _data;
	}

	void push_back(const T & t);

	void push_back(T && t);

	void pop_back(void)
	{
		_size--;
		_data[_size].~T();
	}

	void insert(size_t at, const T & t);

	void erase(size_t at, size_t n = 1);

	T * insert(T * at, const T & t);

	template <typename ...P>
	void emplace_back(const P&... p);
protected:
	T * add_back(void);
};


template <class T>
void vector<T>::reserve(size_t n)
{
	if (n > _capacity)
	{
		_capacity = n;
		T * d = (T *)malloc(_capacity * sizeof(T));
		for(size_t i=0; i<_size; i++)
		{
			new (d + i)T(move(_data[i]));
			_data[i].~T();
		}
		free(_data);
		_data = d;
	}
}

template <class T>
void vector<T>::resize(size_t n)
{
	if (n < _size)
	{
		for(size_t i=n; i<_size; i++)
			_data[i].~T();			
		_size = n;
	}
	else if (n < _capacity)
	{
		for(size_t i=_size; i<n; i++)
			new(_data + i)T();
		_size = n;
	}
	else
	{
		reserve(n);
		_size = n;
	}
}

template <class T>
void vector<T>::shrink_to_fit(void)
{
	if (_size < _capacity)
	{
		_capacity = _size;
		T * d = (T *)malloc(_capacity * sizeof(T));
		for(size_t i=0; i<_size; i++)
		{
			new (d + i)T(move(_data[i]));
			_data[i].~T();
		}
		free(_data);
		_data = d;
	}
}

template <class T>
T * vector<T>::add_back(void)
{
	if (_size == _capacity)
		reserve(_size + 1 + (_size >> 1));
	return _data + _size++;
}

template <class T>
void vector<T>::push_back(const T & t)
{
	new (add_back())T(t);
}

template <class T>
void vector<T>::push_back(T && t)
{
	new (add_back())T(t);
}

template <class T>
template <typename ...P>
void vector<T>::emplace_back(const P&... p)
{
	new (add_back())T(p...);
}

template <class T>
void vector<T>::insert(size_t at, const T & t)
{
	if (_size == _capacity)
		reserve(_size + 1 + (_size >> 1));
	new (_data + _size)T();
	for(size_t i=_size; i>at; i--)
		_data[i] = move(_data[i - 1]);
	_data[at] = t;
	_size++;
}

template <class T>
void vector<T>::erase(size_t at, size_t n)
{
	_size -= n;
	for(size_t i=at; i<_size; i++)
		_data[i] = move(_data[i + n]);
	for(size_t i=0; i<n; i++)
		_data[_size + i].~T();
}

template <class T>
T * vector<T>::insert(T * at, const T & t)
{
	if (_size == _capacity)
	{
		unsigned f = unsigned(at) - unsigned(_data);
		reserve(_size + 1 + (_size >> 1));
		at = (T *)(f + unsigned(_data));
	}
	T * dp = _data + _size;
	new (dp)T();
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
