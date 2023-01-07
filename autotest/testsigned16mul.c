#include <assert.h>

int multab[32];

inline void fill_mulli(int m)
{
#pragma unroll(full)
	for(int i=-16; i<16; i++)
		multab[i + 16] = m * i;
}

inline void check_mulli(int m)
{
	for(int i=-16; i<16; i++)
		assert(multab[i + 16] == m * i);	
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
