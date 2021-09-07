#include <stdio.h>
#include <assert.h>

int fg[15] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610};

int main(void)
{
	int fl[15] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610};

	int	sg = 0, sl = 0;

	for(int i=0; i<15; i++)
	{
		sl += fl[i];
		sg += fg[i];
	}

	assert(sl == 1596);
	assert(sg == 1596);
	
	printf("%d %d\n", sl, sg);
	return 0;
}