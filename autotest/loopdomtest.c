#include <stdio.h>
#include <assert.h>


int main(void)
{
	int	a[10][10];
	
	for(int i=0; i<10; i++)
	{
		for(int j=0; j<10; j++)
		{
			a[i][j] = i + j;
		}
	}
	
	int	s = 0;
	
	for(int i=0; i<10; i++)
	{
		s += a[i][i];
	}
	
	
	assert(s == 90);
	
	return 0;
}
