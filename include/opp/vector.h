#ifndef OPP_VECTOR_H
#define OPP_VECTOR_H

#include <stdlib.h>
#include <opp/move.h>

namespace opp {

template <class T>
class vector
{
protected:
	T	*	_data;
	int		_size, _capacity;
public:
	typedef T 	element_type;

	vector(void) : _data(nullptr), _size(0), _capacity(0) {}

	vector(int n) : _data((T*)malloc(n * sizeof(T))), _size(n), _capacity(n) 
	{
		for(int i=0; i<n; i++)
			new (_data + i) T;
	}

	vector(const vector & v)
		: _data((T*)malloc(v._size * sizeof(T))), _size(v._size), _capacity(v._size) 
	{
		for(int i=0; i<_size; i++)
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
		for(int i=0; i<_size; i++)
			_data[i].~T();
		free(_data);
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

	void resize(int n);

	void reserve(int n);

	void shrink_to_fit(void);

	T & at(int at)
	{
		return _data[at];
	}

	const T & at(int at) const
	{
		return _data[at];
	}

	T & operator[](int at)
	{
		return _data[at];
	}

	const T & operator[](int at) const
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

	void insert(int at, const T & t);

	void erase(int at, int n = 1);

	template <typename ...P>
	void emplace_back(const P&... p);
protected:
	T * add_back(void);
};


template <class T>
void vector<T>::reserve(int n)
{
	if (n > _capacity)
	{
		_capacity = n;
		T * d = (T *)malloc(_capacity * sizeof(T));
		for(int i=0; i<_size; i++)
		{
			new (d + i)T(move(_data[i]));
			_data[i].~T();
		}
		free(_data);
		_data = d;
	}
}

template <class T>
void vector<T>::resize(int n)
{
	if (n < _size)
	{
		for(int i=n; i<_size; i++)
			_data[i].~T();			
		_size = n;
	}
	else if (n < _capacity)
	{
		for(int i=_size; i<n; i++)
			new(_data + i)T;
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
		for(int i=0; i<_size; i++)
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
void vector<T>::insert(int at, const T & t)
{
	if (_size == _capacity)
		reserve(_size + 1 + (_size >> 1));
	new (_data + _size)T;
	for(int i=_size; i>at; i--)
		_data[i] = move(_data[i - 1]);
	_data[at] = t;
}

template <class T>
void vector<T>::erase(int at, int n)
{
	_size -= n;
	for(int i=at; i<_size; i++)
		_data[i] = move(_data[i + n]);
	_data[_size].~T();
}

}
#endif
