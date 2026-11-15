#include <assert.h>


unsigned char a[8];
unsigned char lo = 4;

__noinline void dec2(void)
{
	unsigned char n = 7;
	for (unsigned char v = 7; v >= lo; v--)
		n--;
	assert(n == 3);
}

__noinline void dec2a(void)
{
	unsigned char n = 7;
	for (unsigned char v = 7; v >= lo; v--)
		a[n--] = v;
	
	assert(n == 3);
	assert(a[7] == 7);
	assert(a[4] == 4);
	assert(a[3] == 0);
}

__noinline void inc1a(void)
{
	unsigned char n = 1;
	for (unsigned char v = 7; v >= 4; v--)
	{
		n++;
		a[n] = v;
	}
	assert(n == 5);
}

__noinline void postinc1a(void)
{
	unsigned char n = 0;
	for (unsigned char v = 7; v >= lo; v--)
		a[n++] = v;
	
	assert(n == 4);
	assert(a[0] == 7);
	assert(a[3] == 4);
	assert(a[4] == 0);
}

__noinline int loopendinc(int k)
{
	int n = 2;

	for(;;)
	{
		k -= 1;
		if (!k)
			return n;
		n+=4;
	}
}

void prepa(void)
{
	for(int i=0; i<8; i++) a[i] = 0;
}

char reg[4];
char seen[200];
char tab[16];
unsigned n;

__noinline void work(void)
{
	char i;
	seen[n] = reg[3];
	n++;
	for (i = 0; i < 16; i++)
		tab[i]++;
}

__noinline void testwork(void)
{
	char x = 0x80;
	unsigned i;
	int bad;

	do
	{
		reg[3] = x;
		work();
		x++;
	} while (x & 0x80);
	reg[3] = x;
	work();

	bad = n != 129;
	for (i = 0; i < 128; i++)
		assert(seen[i] == (char)(0x80 + i));

	assert(seen[128] == 0);
}

unsigned n;

__noinline void f(unsigned char x)
{
	n++;
}

__noinline bool loopsbit(void)
{
	unsigned char x = 0;
	do
	{
		f(x);
		if (n > 1000)
			return false;
		x -= -255;
	} while (x & 0x80);

	return n == 129;
}

int main(void)
{
	dec2();

	prepa();
	dec2a();	

	prepa();
	inc1a();	

	prepa();
	postinc1a();	

	assert(loopendinc(5) == 18);

	testwork();

	n = 0;
	assert(loopsbit());

	return 0;
}
