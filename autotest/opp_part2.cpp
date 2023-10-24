#include "opp_part2.h"

BS::BS(const opp::vector<A> & v)
		: va(v)
		{}

int BS::sum(void)
{
	int s = 0;
	for(const auto & a : va)
		s += a.sum();
	return s;
}
