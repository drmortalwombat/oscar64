#ifndef OPP_ALGORITHM_H
#define OPP_ALGORITHM_H

namespace opp {

template <class T>
inline void swap(T & x, T & y)
{
	T t(x); x = y; y = t;
}

template<class T, class LT>
void sort(T s, T e)
{
	while (s != e)
	{
		auto p = s;
		auto q = s;

		q++;
		while (q != e)
		{
			if (LT(*q, *p))
			{
				swap(*q, *p);
				p++;
				swap(*q, *p);				
			}
			q++;
		}

		sort<T, LT>(s, p);
		p++;
		s = p;
	}
}

template<class T, class LF>
void sort(T s, T e, LF lt)
{
	while (s != e)
	{
		auto p = s;
		auto q = s;

		q++;
		while (q != e)
		{
			if (lt(*q, *p))
			{
				swap(*q, *p);
				p++;
				swap(*q, *p);				
			}
			q++;
		}

		sort(s, p, lt);
		p++;
		s = p;
	}
}

template<class II, class OI>
OI copy(II first, II last, OI result)
{
	while (first != last)
	{
		*result = *first;
		++result; ++first;
	}
	return result;
}

}

#endif
