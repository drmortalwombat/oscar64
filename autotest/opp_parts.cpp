#include <assert.h>

#include "opp_part1.h"
#include "opp_part2.h"


int main(void)
{
	opp::vector<A>	va;
	va.push_back(A(1, 2));
	va.push_back(A(3, 4));
	va.push_back(A(6, 4));
	va.push_back(A(0, 9));

	AS	as(va);

	va.push_back(A(7, 1));

	BS 	bs(va);

	assert(bs.sum() == 2 + 12 + 24 + 7);
	assert(as.sum() == 2 + 12 + 24);

	return 0;
}
