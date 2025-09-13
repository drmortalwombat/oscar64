#include <assert.h>



void check_byte(char a)
{
	char b1[8] = {0}, n1[8] = {0}, b0[8] = {0}, n0[8] = {0};

#for(i, 8)	if ((a & (1 << i)) == (1 << i)) b1[i]++;
#for(i, 8)	if ((a & (1 << i)) != (1 << i)) n1[i]++;
#for(i, 8)	if ((a & (1 << i)) == 0) b0[i]++;
#for(i, 8)	if ((a & (1 << i)) != 0) n0[i]++;

	for(char i=0; i<8; i++)
	{
		assert(b0[i] == n1[i]);
		assert(b1[i] == n0[i]);
	}
}

int main(void)
{
	for(int i=0; i<256; i++)
		check_byte(i);
}