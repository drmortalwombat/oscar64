#include <opp/vector.h>
#include <opp/algorithm.h>
#include <assert.h>
#include <opp/iostream.h>

int main(void)
{
	opp::vector<int>		a;

	for(int i=0; i<10; i++)
		a.push_back(i);

	int s = 0;
	for(int i=0; i<a.size(); i++)
		s += a[i];

	assert(s == 45);

	for(int i=0; i<5; i++)
		a.erase(i);

	s = 0;
	for(int i=0; i<a.size(); i++)
		s += a[i];

	assert(s == 1 + 3 + 5 + 7 + 9);

	opp::vector<int>	v;

	for(int i=0; i<10; i++)
		v.push_back(i);

	assert(v.size() == 10);
	v.insert(0, 20);
	assert(v.size() == 11);
	v.insert(6, 21);
	assert(v.size() == 12);
	v.insert(12, 22);

	int * fi = opp::find(v.begin(), v.end(), 21);

	fi = v.insert(fi, 30);
	fi = v.insert(fi, 31);
	fi = v.insert(fi, 32);

	assert(v.size() == 16);
	assert(v[0] == 20);
	assert(v[15] == 22);
	assert(v[8] == 32);

	fi = opp::find(v.begin(), v.end(), 32);

	for(int i=0; i<30; i++)
	{
		fi = v.insert(fi, i + 40);
	}

	assert(v.size() == 46);
	assert(v[28] == 60);

	v.erase(10, 10);

	for(int i : v) 
		opp::cout << i << ", ";
	opp::cout << "\n";

	return 0;
}
