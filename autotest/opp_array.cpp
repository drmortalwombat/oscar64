#include <opp/array.h>
#include <assert.h>

int main(void)
{
	opp::array<int, 10>	a10;
	opp::array<int, 20>	a20;

	for(int i=0; i<10; i++)
		a10[i] = i;
	for(int i=0; i<20; i++)
		a20[i] = i;

	int s = 0;
	for(int i=0; i<10; i++)
		s += a10[i];
	for(int i=10; i<20; i++)
		s -= a20[i];

	assert(s == -100);

	return 0;
}
