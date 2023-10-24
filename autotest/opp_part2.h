#pragma once

#include "opp_part1.h"

class BS
{
protected:
	opp::vector<A>	va;
public:
	BS(const opp::vector<A> & v);
	int sum(void);
};



#pragma compile("opp_part2.cpp")
