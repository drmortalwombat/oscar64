#include <string.h>
#include <assert.h>

int main(void)
{
	assert(strcmp("ABCD", "ABCD") == 0);
	assert(strcmp("ABCE", "ABCD") == 1);
	assert(strcmp("ABCD", "ABCE") == -1);
	
	return 0;
}
