#include <opp/vector.h>
#include <opp/string.h>
#include <assert.h>
#include <stdlib.h>
#include <opp/iostream.h>

using opp::string;
using opp::vector;

string join(const vector<string> & vs)
{
	string	sj;
	for(int i=0; i<vs.size(); i++)
		sj += vs[i];
	return sj;
}

int main(void)
{
	vector<string>	vs;
	string			a;

	for(int i=0; i<10; i++)
	{
		vs.push_back(a);
		a += "x";
	}

	int s = 0;
	for(int i=0; i<10; i++)
		s += vs[i].size();

	assert(s == 45);

	assert(join(vs).size() == 45);

	return 0;
}
