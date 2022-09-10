// randsumtest	

#include <stdlib.h>
#include <assert.h>

int main(void)
{
	long	lsum = 0;
	for(unsigned i=0; i<1000; i++)
		lsum += rand();
	
	assert(lsum == 32157742L);
	
	return 0;
}

