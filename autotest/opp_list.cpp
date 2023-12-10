#include <opp/list.h>
#include <opp/algorithm.h>
#include <assert.h>
#include <opp/iostream.h>

int main(void)
{
	opp::list<int>		a;

	for(int i=0; i<10; i++)
		a.push_back(i);

	int s = 0;
	for(auto i : a)
		s += i;

	assert(s == 45);

	auto li = a.begin();
	for(int i=0; i<5; i++)
	{
		li = a.erase(li);
		li++;
	}

	s = 0;
	for(auto i : a)
		s += i;

	assert(s == 1 + 3 + 5 + 7 + 9);

	opp::list<int>		b;

	b = a;

	s = 0;
	for(auto i : b)
		s += i;

	assert(s == 1 + 3 + 5 + 7 + 9);

	opp::list<int>		c = opp::list<int>(b);

	s = 0;
	for(auto i : c)
		s += i;

	assert(s == 1 + 3 + 5 + 7 + 9);

	s = 0;
	for(auto i : b)
		s += i;

	assert(s == 1 + 3 + 5 + 7 + 9);

	return 0;
}
