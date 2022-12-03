#include <assert.h>


int a100[100];
__striped int b100[100];
__striped int c100[100];

unsigned a256[256];
__striped unsigned b256[256];
__striped unsigned c256[256];

#pragma align(c100, 256)
#pragma align(c256, 256)

void test_ab100(void)
{
	for(char i=0; i<100; i++)
	{
		a100[i] = i * i;
		b100[i] = i * i;
		c100[i] = i * i;
	}

	a100[31] = 4711;
	b100[31] = 4711;
	c100[31] = 4711;


	for(char i=0; i<100; i++)
	{
		a100[i] += i; a100[i] -= 5; a100[i] = a100[i] + a100[99 - i];
		b100[i] += i; b100[i] -= 5; b100[i] = b100[i] + b100[99 - i];
		c100[i] += i; c100[i] -= 5; c100[i] = c100[i] + c100[99 - i];
	}

	for(char i=0; i<100; i++)
	{
		assert(a100[i] == b100[i]);
		assert(a100[i] == c100[i]);
	}
}

void test_ab256(void)
{
	for(unsigned i=0; i<256; i++)
	{
		a256[i] = i * i;
		b256[i] = i * i;
		c256[i] = i * i;
	}

	a256[31] = 4711;
	b256[31] = 4711;
	c256[31] = 4711;

	for(unsigned i=0; i<256; i++)
	{
		a256[i] += i; a256[i] -= 5; a256[i] = a256[i] + a256[255 - i];
		b256[i] += i; b256[i] -= 5; b256[i] = b256[i] + b256[255 - i];
		c256[i] += i; c256[i] -= 5; c256[i] = c256[i] + c256[255 - i];
	}

	for(unsigned i=0; i<256; i++)
	{
		assert(a256[i] == b256[i]);
		assert(a256[i] == c256[i]);
	}
}

long la50[50];
__striped long lb50[50];
__striped long lc50[50];

unsigned long la256[256];
__striped unsigned long lb256[256];
__striped unsigned long lc256[256];

#pragma align(lc50, 256)
#pragma align(lc256, 256)

void test_lab50(void)
{
	for(char i=0; i<50; i++)
	{
		long j = i * i;
		la50[i] = j * j;
		lb50[i] = j * j;
		lc50[i] = j * j;
	}

	la50[31] = 47110815l;
	lb50[31] = 47110815l;
	lc50[31] = 47110815l;

	for(char i=0; i<50; i++)
	{
		la50[i] += i; la50[i] -= 12345678l; la50[i] = la50[i] + la50[49 - i];
		lb50[i] += i; lb50[i] -= 12345678l; lb50[i] = lb50[i] + lb50[49 - i];
		lc50[i] += i; lc50[i] -= 12345678l; lc50[i] = lc50[i] + lc50[49 - i];
	}

	for(char i=0; i<50; i++)
	{
		assert(la50[i] == lb50[i]);
		assert(la50[i] == lc50[i]);
	}
}

void test_lab256(void)
{
	for(unsigned i=0; i<256; i++)
	{
		unsigned long j = i * i;
		la256[i] = j * j;
		lb256[i] = j * j;
		lc256[i] = j * j;
	}

	la256[31] = 47110815ul;
	lb256[31] = 47110815ul;
	lc256[31] = 47110815ul;

	for(unsigned i=0; i<256; i++)
	{
		la256[i] += i; la256[i] -= 12345678ul; la256[i] = la256[i] + la256[255 - i];
		lb256[i] += i; lb256[i] -= 12345678ul; lb256[i] = lb256[i] + lb256[255 - i];
		lc256[i] += i; lc256[i] -= 12345678ul; lc256[i] = lc256[i] + lc256[255 - i];
	}

	for(unsigned i=0; i<256; i++)
	{
		assert(la256[i] == lb256[i]);
		assert(la256[i] == lc256[i]);
	}
}

float fa50[50];
__striped float fb50[50];
__striped float fc50[50];

float fa256[256];
__striped float fb256[256];
__striped float fc256[256];

#pragma align(fc50, 256)
#pragma align(fc256, 256)

void test_fab50(void)
{
	for(char i=0; i<50; i++)
	{
		fa50[i] = i * i;
		fb50[i] = i * i;
		fc50[i] = i * i;
	}

	fa50[31] = 4711.0815;
	fb50[31] = 4711.0815;
	fc50[31] = 4711.0815;

	for(char i=0; i<50; i++)
	{
		fa50[i] += i; fa50[i] -= 1234.5678; fa50[i] = fa50[i] + fa50[49 - i];
		fb50[i] += i; fb50[i] -= 1234.5678; fb50[i] = fb50[i] + fb50[49 - i];
		fc50[i] += i; fc50[i] -= 1234.5678; fc50[i] = fc50[i] + fc50[49 - i];
	}

	for(char i=0; i<50; i++)
	{
		assert(fa50[i] == fb50[i]);
		assert(fa50[i] == fc50[i]);
	}
}

void test_fab256(void)
{
	for(unsigned i=0; i<256; i++)
	{
		fa256[i] = i * i;
		fb256[i] = i * i;
		fc256[i] = i * i;
	}

	fa256[31] = 4711.0815;
	fb256[31] = 4711.0815;
	fc256[31] = 4711.0815;

	for(unsigned i=0; i<256; i++)
	{
		fa256[i] += i; fa256[i] -= 1234.5678; fa256[i] = fa256[i] + fa256[255 - i];
		fb256[i] += i; fb256[i] -= 1234.5678; fb256[i] = fb256[i] + fb256[255 - i];
		fc256[i] += i; fc256[i] -= 1234.5678; fc256[i] = fc256[i] + fc256[255 - i];
	}

	for(unsigned i=0; i<256; i++)
	{
		assert(fa256[i] == fb256[i]);
		assert(fa256[i] == fc256[i]);
	}
}


unsigned	da50[50], db50[50], dc50[50];

unsigned * pa50[50];
__striped unsigned * pb50[50];
__striped unsigned * pc50[50];

#pragma align(pc50, 256)

void test_pab50(void)
{
	for(char i=0; i<50; i++)
	{
		pa50[i] = da50 + (i * 17) % 50;
		pb50[i] = db50 + (i * 17) % 50;
		pc50[i] = dc50 + (i * 17) % 50;
	}

	for(char i=0; i<50; i++)
	{
		*pa50[i] = i * i;
		*pb50[i] = i * i;
		*pc50[i] = i * i;
	}

	for(char i=0; i<50; i++)
	{
		*pa50[i] += i; *pa50[i] -= 5; *pa50[i] = *pa50[i] + *pa50[49 - i];
		*pb50[i] += i; *pb50[i] -= 5; *pb50[i] = *pb50[i] + *pb50[49 - i];
		*pc50[i] += i; *pc50[i] -= 5; *pc50[i] = *pc50[i] + *pc50[49 - i];
	}

	for(char i=0; i<50; i++)
	{
		assert(*pa50[i] == *pb50[i]);
		assert(*pa50[i] == *pc50[i]);
		assert(da50[i] == db50[i]);
		assert(da50[i] == dc50[i]);
	}
}

unsigned	da50_4[50][4], db50_4[50][4], dc50_4[50][4];

void test_pab50_4(void)
{
	for(char i=0; i<50; i++)
	{
		pa50[i] = da50_4[(i * 17) % 50];
		pb50[i] = db50_4[(i * 17) % 50];
		pc50[i] = dc50_4[(i * 17) % 50];
	}

	for(char k=0; k<4; k++)
	{
		for(char i=0; i<50; i++)
		{
			pa50[i][k] = i * i;
			pb50[i][k] = i * i;
			pc50[i][k] = i * i;
		}
	}

	for(char k=0; k<4; k++)
	{
		for(char i=0; i<50; i++)
		{
			pa50[i][k] += i; pa50[i][k] -= 5; pa50[i][k] = pa50[i][k] + pa50[49 - i][k];
			pb50[i][k] += i; pb50[i][k] -= 5; pb50[i][k] = pb50[i][k] + pb50[49 - i][k];
			pc50[i][k] += i; pc50[i][k] -= 5; pc50[i][k] = pc50[i][k] + pc50[49 - i][k];
		}
	}

	for(char k=0; k<4; k++)
	{
		for(char i=0; i<50; i++)
		{
			assert(pa50[i][k] == pb50[i][k]);
			assert(pa50[i][k] == pc50[i][k]);
			assert(da50_4[i][k] == db50_4[i][k]);
			assert(da50_4[i][k] == dc50_4[i][k]);
		}
	}
}


int main(void)
{
	test_ab100();
	test_ab256();
	test_lab50();
	test_lab256();
	test_fab50();
	test_fab256();
	test_pab50();
	test_pab50_4();

	return 0;
}
