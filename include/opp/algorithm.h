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

}

#endif
