#ifndef OPP_ITERATOR_H
#define OPP_ITERATOR_H

namespace opp
{

template <class CT>
class back_insert_iterator
{
protected:
	CT 	*	co;

public:
	back_insert_iterator (CT & c) : co(&c) {}

	back_insert_iterator & operator= (const CT::element_type & t)
	{
		co->push_back(t); 
		return *this; 
	}

	back_insert_iterator & operator= (CT::element_type && t)
	{
		co->push_back(t); 
		return *this; 
	}

	back_insert_iterator & operator* (void)
	{
		return *this; 
	}

	back_insert_iterator & operator++ (void)
	{
		return *this;
	}

	back_insert_iterator operator++ (int)
	{
		return *this;
	}
};

template <class CT>
class front_insert_iterator
{
protected:
	CT 	*	co;

public:
	front_insert_iterator (CT & c) : co(&c) {}

	front_insert_iterator & operator= (const CT::element_type & t)
	{
		co->push_front(t); 
		return *this; 
	}

	front_insert_iterator & operator= (CT::element_type && t)
	{
		co->push_front(t); 
		return *this; 
	}

	front_insert_iterator & operator* (void)
	{
		return *this; 
	}

	front_insert_iterator & operator++ (void)
	{
		return *this;
	}

	front_insert_iterator operator++ (int)
	{
		return *this;
	}
};

template <class CT>
class insert_iterator
{
protected:
	CT 					*	co;
	CT::iterator_type		ci;

public:
	insert_iterator (CT & c, const CT::iterator_type & i) : co(&c), ci(i) {}

	insert_iterator & operator= (const CT::element_type & t)
	{
		ci = co->insert(ci, t); ++ci;
		return *this; 
	}

	insert_iterator & operator= (CT::element_type && t)
	{
		ci = co->insert(ci, t); ++ci;
		return *this; 
	}

	insert_iterator & operator* (void)
	{
		return *this; 
	}

	insert_iterator & operator++ (void)
	{
		return *this;
	}

	insert_iterator operator++ (int)
	{
		return *this;
	}
};

template <class T>
class ostream_iterator
{
protected:
	ostream		&	stream;
	const char	*	str;
public:
	ostream_iterator(ostream & stream_, const char * str_)
		: stream(stream_), str(str_) {}

	ostream_iterator & operator= (const T & t)
	{
		stream << t;
		if (str)
			stream << str;
		return *this; 
	}

	ostream_iterator & operator= (T && t)
	{
		stream << t;
		if (str)
			stream << str;
		return *this; 
	}

	ostream_iterator & operator* (void)
	{
		return *this; 
	}

	ostream_iterator & operator++ (void)
	{
		return *this;
	}

	ostream_iterator operator++ (int)
	{
		return *this;
	}
};

template <class CT>  
front_insert_iterator<CT> front_inserter (CT & c)
{
	return front_insert_iterator<CT>(c);
}

template <class CT>  
back_insert_iterator<CT> back_inserter (CT & c)
{
	return back_insert_iterator<CT>(c);
}

template <class CT>  
insert_iterator<CT> inserter (CT & c, const CT::iterator_type & i)
{
	return insert_iterator<CT>(c, i);
}

template <class CT>  
CT next(CT it, int n = 1)
{
	while (n > 0)
	{
		it++;
		n--;
	}
	return it;
}

template <class CT>  
CT prev(CT it, int n = 1)
{
	while (n > 0)
	{
		it--;
		n--;
	}
	return it;
}

}
