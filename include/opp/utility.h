#ifndef OPP_UTILITY_H
#define OPP_UTILITY_H

namespace opp {

template <class T>
T && move(T & m)
{
	return (T &&)m;
}


template<class T1, class T2>
struct pair
{
	T1	first;
	T2	second;

	pair(T1 && t1, T2 && t2)
		: first(t1), second(t2)
		{}
};

template<class T1, class T2>
constexpr pair<T1, T2> make_pair(T1 && t1, T2 && t2)
{
	return pair<T1, T2>(t1, t2);
}

}



#endif
