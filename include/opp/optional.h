#ifndef OPP_OPTIONAL_H
#define OPP_OPTIONAL_H

#include <opp/utility.h>

namespace opp {

template <class T>
class optional
{
protected:
	char	_data[sizeof(T)];
	bool	valid;
public:
	optional(void) : valid(false) {}
	optional(const T & data) : valid(true) 
	{
		new (_data)T(data);
	}

	optional(T && data) : valid(true) 
	{
		new (_data)T(opp::move(data));
	}

	optional(const optional<T> & o)
		: valid(o.valid)
	{
		if (valid)
			new (_data)T(*((T*)o._data));
	}

	optional(optional<T> && o)
		: valid(o.valid)
	{
		if (valid)
			new (_data)T(opp::move(*((T*)o._data)));
	}

	~optional(void)
	{
		if (valid)
			((T*)_data)->~T();
	}

	optional<T> & operator=(const optional<T> & o)
	{
		if (this != &o)
		{
			if (valid)
				((T*)_data)->~T();
			valid = o.valid;
			if (valid)
				new (_data)T(*((T*)o._data));
		}
		return *this;
	}

	optional<T> & operator=(optional<T> && o)
	{
		if (this != &o)
		{
			if (valid)
				((T*)_data)->~T();
			valid = o.valid;
			if (valid)
				new (_data)T(opp::move(*((T*)o._data)));
		}
		return *this;
	}

	operator bool(void) const {return valid;}
	T & operator *(void) const {return *(T*)_data;}
	const T & operator *(void) const {return *(T*)_data;}

	const T * operator->(void) const {return (T*)_data;}

	T * begin(void)
	{
		return (T*)_data;
	}

	const T * begin(void) const
	{
		return (T*)_data;
	}

	const T * cbegin(void) const
	{
		return (T*)_data;
	}

	T * end(void)
	{
		return (T*)_data + (valid ? 1 : 0);
	}

	const T * end(void) const
	{
		return (T*)_data + (valid ? 1 : 0);
	}

	const T * cend(void) const
	{
		return (T*)_data + (valid ? 1 : 0);
	}

};

}

#endif
