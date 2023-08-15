#ifndef OPP_ARRAY_H
#define OPP_ARRAY_H

namespace opp {

template <class T, int n>
class array
{
protected:
	T	_data[n];
public:
	int size(void) const
	{
		return n;
	}

	int max_size(void) const
	{
		return n;
	}

	bool empty(void) const
	{
		return n == 0;
	}

	const T & at(int at) const
	{
		return _data[at];
	}

	T & at(int at)
	{
		return _data[at];
	}

	T & operator[] (int at)
	{
		return _data[at];
	}

	const T & operator[] (int at) const
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
		return _data + n;
	}

	const T * end(void) const
	{
		return _data + n;
	}

	const T * cend(void) const
	{
		return _data + n;
	}

	T & back(void)
	{
		return _data[n - 1];
	}

	const T & back(void) const
	{
		return _data[n - 1];
	}

	T & front(void)
	{
		return _data[0];
	}

	const T & front(void) const
	{
		return _data[0];
	}

	T * data(void)
	{
		return _data;
	}

	const T * data(void) const
	{
		return _data;
	}

	void fill(const T & t)
	{
		for(int i=0; i<n; i++)
			_data[i] = t;
	}
};

}


#endif

