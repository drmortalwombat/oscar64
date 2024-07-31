#include <string.h>
#include <assert.h>

char lstr[1025];

int main(void)
{
#if 1
	assert(strlen("") == 0);
	assert(strlen("1") == 1);
	assert(strlen("12") == 2);
	assert(strlen("123") == 3);
	assert(strlen("1234") == 4);
	assert(strlen("12345") == 5);
	assert(strlen("123456") == 6);
#endif
#if 1
	char * dp = lstr;
	for(int i=0; i<1024; i++)
	{
		*dp = 0;
		assert(strlen(lstr) == i);
		*dp++ = 'a';
	}
#endif
	return 0;
}
