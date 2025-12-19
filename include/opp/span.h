#ifndef OPP_SPAN_H
#define OPP_SPAN_H

#include <stddef.h>
#include <stdlib.h>
#include <opp/utility.h>

namespace opp
{
static const size_t	dynamic_extent	=	0xffff;

template <class T, int N = dynamic_extent>
class span
{
protected:
	T		*	_data;
	size_t		_size;
public:
	span(void)
		: _data(nullptr), _size(0)
		{}

	span(T * data, size_t size)
		: _data(data), _size(size)
		{}

	span(T * data)
		: _data(data), _size(N)
		{}

	span(opp::array<T, N> & arr)
		: _data(arr.data()), _size(N)
		{}

	span(opp::vector<T> & vec)
		: _data(vec.data()), _size(vec.size())
		{}

	span(const opp::array<T, N> & arr)
		: _data(arr.data()), _size(N)
		{}

	span(const opp::vector<T> & vec)
		: _data(vec.data()), _size(vec.size())
		{}

	constexpr size_t size(void) const
	{
		if constexpr (N == dynamic_extent)
			return _size;
		else
			return N;
	}

	T & operator[](int i) 
		{return _data[i];}

	const T & operator[](int i) const
		{return _data[i];}

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
		return _data + size();
	}

	const T * end(void) const
	{
		return _data + size();
	}

	const T * cend(void) const
	{
		return _data + size();
	}

	T & back(void)
	{
		return _data[size() - 1];
	}

	const T & back(void) const
	{
		return _data[size() - 1];
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

	template< int Offset, int Count = dynamic_extent >
	constexpr auto subspan() const
	{
		if constexpr (Count == dynamic_extent)
		{
			if constexpr (N == dynamic_extent)
				return opp::span<T>(_data + Offset, _size - Offset);
			else
				return opp::span<T, (N - Offset)>(_data + Offset);
		}
		else
			return opp::span<T, Count>(_data + Offset);
	}

	constexpr auto subspan(size_t offset, size_t count = dynamic_extent)
	{
		if (count == dynamic_extent)
			return opp::span<T>(_data + offset, size() - offset);
		else
			return opp::span<T>(_data + offset, count);
	}

	template< int Count >
	constexpr auto first() const
	{
		return opp::span<T, Count>(_data);
	}

	constexpr auto first( size_t count ) const
	{
		return opp::span<T>(_data, count);
	}

	template< int Count >
	constexpr auto last() const
	{
		return opp::span<T, Count>(_data + size() - Count);
	}

	constexpr auto last( size_t count ) const
	{
		return opp::span<T>(_data + size() - count, count);
	}
};
	
}


#endif
