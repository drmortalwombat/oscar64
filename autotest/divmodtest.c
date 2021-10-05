#include <assert.h>

int main(void)
{
	for(unsigned i=0; i<256; i+=11)
	{
		for(unsigned j=1; j<256; j++)
		{
			unsigned q = i / j, r = i % j;
			
			assert(q * j + r == i);
			assert(r >= 0 && r < j);
		}
	}

	for(unsigned i=0; i<7000; i+=11)
	{
		for(unsigned j=1; j<i; j*=3)
		{
			unsigned q = i / j, r = i % j;
			
			assert(q * j + r == i);
			assert(r >= 0 && r < j);
		}
	}
	
	return 0;
}
