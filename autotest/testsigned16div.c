#include <assert.h>

int multab[32];

void fill_mulli(int m)
{
#pragma unroll(full)
	for(int i=-16; i<16; i++)
		if (i != 0)
			multab[i + 16] = m / i;
}

void check_mulli(int m)
{
	for(int i=-16; i<16; i++)
		if (i != 0)
			assert(multab[i + 16] == m / i);	
}

int main(void)
{
	for(int i=-1024; i<=1024; i++)
	{
		fill_mulli(i);
		check_mulli(i);
	}

	return 0;
}
