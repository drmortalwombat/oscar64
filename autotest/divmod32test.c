#include <assert.h>

void check(unsigned long l, unsigned long r)
{
	unsigned long d = l / r, m = l % r;

	assert(d * r + m == l);
	assert(m < r);
}

int main(void)
{
	for(char i=0; i<28; i++)
	{
		for(char j=0; j<28; j++)
		{
			check(0xb3ul << i, 0x2bul << j);
			check(0xb3ul << i, 0x01ul << j);
			check(0x01ul << i, 0xc2ul << j);
			check(0xb31ful << i, 0x2bul << j);
			check(0xb354ul << i, 0x01ul << j);
			check(0xb3ul << i, 0x2b1cul << j);
			check(0xb3ul << i, 0x013ful << j);
			check(0xb31ful << i, 0x2b23ul << j);
			check(0xb354ul << i, 0x0145ul << j);
			check(0xb31f24ul << i, 0x2bul << j);
			check(0xb35421ul << i, 0x01ul << j);
			check(0xb31f24ul << i, 0x2b23ul << j);
			check(0xb35421ul << i, 0x0145ul << j);
			check(0xb31f24ul << i, 0x2b2356ul << j);
			check(0xb35421ul << i, 0x0145a7ul << j);
		}
	}

	return 0;
}
