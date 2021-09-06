#include <stdio.h>
#include <assert.h>

int option(bool a, int b, int c)
{
	return a ? b : c;
}

int main(void)
{
	assert(option(true, 1, 2) == 1);
	assert(option(false, 1, 2) == 2);
	
	return 0;
}