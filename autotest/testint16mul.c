#include <assert.h>

int mul[32][32];

int main(void)
{
	for(int i=0; i<32; i++)
		for(int j=0; j<32; j++)
			mul[i][j] = i * j;
#assign xi 0
#repeat

	for(int j=0; j<32; j++)
	{
		assert(mul[xi][j] == xi * j);
		assert(mul[j][xi] == j * xi);
	}
	
#assign xi xi + 1
#until xi == 32

	return 0;
}
