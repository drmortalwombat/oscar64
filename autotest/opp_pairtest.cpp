#include <assert.h>
#include <opp/vector.h>
#include <opp/utility.h>
#include <opp/iostream.h>

using namespace opp;

int main(void)
{
	vector<pair<int, int> >	vii;

	for(int i=0; i<100; i++)
		vii.push_back(make_pair(i, i * i));

	int 	sum1 = 0;
	long	sum2 = 0;
	for(const auto & v : vii)
	{
		sum1 += v.first;
		sum2 += v.second;
	}


	assert(sum1 ==   4950);
	assert(sum2 == 328350l);

	return 0;
}
