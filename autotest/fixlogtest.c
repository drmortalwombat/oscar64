#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

static const char qpow2tab[] = {
	#for(i, 32) (char)floor(pow(2.0, float(i) / 32.0) * 256.0 + 0.5),
};

static const char qlog2tab[] = {
	#for(i, 32) (char)floor(32.0 * log(1.0 + float(i) / 32.0) / log(2.0) + 0.5),
};

static const char qlog2tabs[] = {
	0, 
	#for(i, 31) (char)floor(32.0 * log(float(i + 1.0) / 256.0) / log(2.0) + 0.5 + 256.0),
};

unsigned qpow2(char p)
{
	unsigned u = qpow2tab[p & 31] | 0x100;
	return u << (p >> 5);
}

unsigned qpow2i(int i)
{
	unsigned u = qpow2(i & 255);

	switch ((char)(i >> 8))
	{
	case 0xff:
		return u >> 8;
	case 0:
		return u;
	case 1 ... 127:
		return 0xffff;
	default:
		return 0;
	}
}

uint32_t qpow2l(int i)
{
	unsigned u = qpow2(i & 255);

	switch ((char)(i >> 8))
	{
	case 0xff:
		return u >> 8;
	case 0:
		return u;
	case 1:
		return (uint32_t)u << 8;
	case 2:
		return (uint32_t)u << 16;
	case 3 ... 127:
		return 0xffffffff;
	default:
		return 0;
	}
}

__striped static const int biglogtab[11] = {
	#for(i, 11) 32 * (i - 3),
};


__native int qlog2i(unsigned u)
{
	if (u < 32)
		return qlog2tabs[u] - 256;
	else
	{
		char	s = 0;
		while (u >= 256)
		{
			u >>= 1;
			s++;
		}

		while (u >= 64)
		{
			u >>= 1;
			s++;
		}

		return qlog2tab[u - 32] + biglogtab[s];
	}
}

int qlog2l(uint32_t l)
{
	if (l >= 0x01000000)
		return qlog2i(l >> 16) + 512;
	else if (l >= 0x00010000)
		return qlog2i(l >> 8) + 256;
	else
		return qlog2i(l);
}

int main(void)
{
	unsigned	u = 1;
	for(char i=0; i<16; i++)
	{
		assert(qlog2i(u) == 32 * i - 256);
		assert(qpow2i(i * 32 - 256) == u);
		u <<= 1;
	}

	uint32_t	ul = 1;
	for(char i=0; i<32; i++)
	{
		assert(qlog2l(ul) == 32 * i - 256);
		assert(qpow2l(i * 32 - 256) == ul);
		ul <<= 1;
	}

	for(int i=0; i<256; i++)
	{
		assert(abs(qlog2i(qpow2i(i)) - i) <= 1);
	}

	for(int i=1; i<256; i++)
	{
		assert(abs(qpow2i(qlog2i(i)) - i) < 8);
	}

	return 0;
}