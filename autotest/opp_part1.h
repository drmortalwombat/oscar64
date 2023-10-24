#pragma once

#include <opp/vector.h>

class A
{
protected:
	int 	a, b;

public:
	A(int a_, int b_)
		: a(a_), b(b_)
		{}

	int sum(void) const
	{
		return a * b;
	}
};

class AS
{
protected:
	opp::vector<A>	va;
public:
	AS(const opp::vector<A> & v)
		: va(v)
		{}

	int sum(void)
	{
		int s = 0;
		for(const auto & a : va)
			s += a.sum();
		return s;
	}
};

#pragma compile("opp_part1.cpp")

