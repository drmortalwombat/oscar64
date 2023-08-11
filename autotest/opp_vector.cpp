#include <opp/vector.h>
#include <assert.h>

int main(void)
{
	vector<int>		a;

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

	return 0;
}
