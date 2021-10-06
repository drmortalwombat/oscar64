#include <string.h>
#include <assert.h>

char	aa[2000], ba[2000];

int main(void)
{
	assert(strcmp("abcdefgh", "abcdefgh") == 0);
	assert(strcmp("abcdefgh", "abcdemgh") < 0);
	assert(strcmp("abcdefgh", "abcdefghi") < 0);
	assert(strcmp("abcdefghi", "abcdefgh") > 0);
	assert(strcmp("abcdemgh", "abcdefgh") > 0);

	for(int i=0; i<1900; i++)
	{
		aa[i] = 'a' + (i & 7);
	}
	aa[1900] = 0;
	
	strcpy(ba, aa);
	
	assert(strcmp(aa, ba) == 0);
	ba[1000] = 'z';
	assert(strcmp(aa, ba) < 0);
	assert(strcmp(ba, aa) > 0);
	
	return 0;
}
