#ifndef OPP_VECTOR_H
#define OPP_VECTOR_H

template <class T>
class vector
{
protected:
	T	*	_data;
	int		_size, _capacity;
public:
	vector(void) : _data(nullptr), _size(0), _capacity(0) {}
	vector(int n) : _data(new T[n]), _size(n), _capacity(n) {}
	~vector(void)
	{
		delete[] _data;
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

	void pop_back(void)
	{
		_size--;
	}

	void insert(int at, const T & t);

	void erase(int at, int n = 1);
};


template <class T>
void vector<T>::reserve(int n)
{
	if (n > _capacity)
	{
		_capacity = n;
		T * d = new T[_capacity];
		for(int i=0; i<_size; i++)
			d[i] = _data[i];
		delete[] _data;
		_data = d;
	}
}

template <class T>
void vector<T>::resize(int n)
{
	if (n < _size)
		_size = n;
	else if (n < _capacity)
		_size = n;
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
		T * d = new T[_capacity];
		for(int i=0; i<_size; i++)
			d[i] = _data[i];
		delete[] _data;
		_data = d;
	}
}

template <class T>
void vector<T>::push_back(const T & t)
{
	if (_size == _capacity)
		reserve(_size + 1 + (_size >> 1));
	_data[_size++] = t;
}

template <class T>
void vector<T>::insert(int at, const T & t)
{
	if (_size == _capacity)
		reserve(_size + 1 + (_size >> 1));
	for(int i=_size; i>at; i--)
		_data[i] = _data[i - 1];
	_data[at] = t;
}

template <class T>
void vector<T>::erase(int at, int n)
{
	_size -= n;
	for(int i=at; i<_size; i++)
		_data[i] = _data[i + n];
}

#endif
