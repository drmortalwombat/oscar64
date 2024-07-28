#include <fixmath.h>
#include <assert.h>
#include <stdlib.h>

unsigned	tval[] = {
	1, 2, 16, 128, 255, 256, 4096, 32768, 65535
};

void testmuldiv16u(void)
{
	for (char i=0; i<9; i++)
	{
		assert(lmuldiv16u(tval[i], 0, tval[i]) == 0);
		assert(lmuldiv16u(0, tval[i], tval[i]) == 0);
		for(char j=0; j<9; j++)
		{			
			assert(lmuldiv16u(tval[i], tval[j], tval[i]) == tval[j]);
			assert(lmuldiv16u(tval[j], tval[i], tval[i]) == tval[j]);
		}
	}

	for(int i=0; i<10000; i++)
	{
		unsigned	a = rand();
		unsigned	b = rand();
		unsigned	c = rand();
		if (c > 0)
		{
			unsigned long d = (unsigned long)a * (unsigned long) b / c;
			if (d < 0x10000l)
				assert(lmuldiv16u(a, b, c) == d);
		}
	}
}

unsigned	ival[] = {
	1, 2, 16, 128, 255, 256, 4096, 32767, 
	-1, -2, -16, -128, -255, -256, -4096, -32767
};

void testmuldiv16s(void)
{
	for (char i=0; i<16; i++)
	{
		assert(lmuldiv16s(ival[i], 0, ival[i]) == 0);
		assert(lmuldiv16s(0, ival[i], ival[i]) == 0);
		for(char j=0; j<16; j++)
		{			
			assert(lmuldiv16s(ival[i], ival[j], ival[i]) == ival[j]);
			assert(lmuldiv16s(ival[j], ival[i], ival[i]) == ival[j]);
		}
	}

	for(int i=0; i<10000; i++)
	{
		int	a = rand();
		int	b = rand();
		int	c = rand();

		if (c > 0)
		{
			long d = (long)a * (long)b / c;
			if (d >= -32768 && d <= 32767)
				assert(lmuldiv16s(a, b, c) == d);
		}
	}

}

void testlmul4f12s(void)
{
	for(int i=0; i<20000; i++)
	{
		int	a = rand();
		int	b = rand();

		long d = ((long)a * (long)b) >> 12;
		if (d >= -32768 && d <= 32767)
			assert(lmul4f12s(a, b) == d);
	}
}

int main(void)
{
	testlmul4f12s();
	testmuldiv16u();
	testmuldiv16s();

	return 0;
}