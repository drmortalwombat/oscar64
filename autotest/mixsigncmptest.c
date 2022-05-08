#include <assert.h>

int main(void)
{
	unsigned	n1, n0;

	n1 = 0; n0 = 0;
	for(int i=-1000; i<2000; i+=37)
	{
		for(char j=0; j<255; j++)
		{
			if (i < j)
				n1++;
			else
				n0++;
		}
	}

	assert(n1 == 7893 && n0 == 13017);

	n1 = 0; n0 = 0;
	for(int i=-1000; i<2000; i+=37)
	{
		for(char j=0; j<255; j++)
		{
			if (i <= j)
				n1++;
			else
				n0++;
		}
	}

	assert(n1 == 7899 && n0 == 13011);

	n1 = 0; n0 = 0;
	for(int i=-1000; i<2000; i+=37)
	{
		for(char j=0; j<255; j++)
		{
			if (i >= j)
				n1++;
			else
				n0++;
		}
	}

	assert(n0 == 7893 && n1 == 13017);

	n1 = 0; n0 = 0;
	for(int i=-1000; i<2000; i+=37)
	{
		for(char j=0; j<255; j++)
		{
			if (i > j)
				n1++;
			else
				n0++;
		}
	}

	assert(n0 == 7899 && n1 == 13011);

	return 0;
}