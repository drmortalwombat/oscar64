#include <assert.h>

template<int n, int m>
void checkmul(void)
{
	__striped static const unsigned	r[256] = {
#for(i, 256) i * n,
	};

	for(unsigned i=0; i<m; i++)
	{
		assert((i &  ~0) * n == r[i]);
		assert((i &  ~1) * n == r[i >> 1] << 1);
		assert((i &  ~3) * n == r[i >> 2] << 2);
		assert((i &  ~7) * n == r[i >> 3] << 3);
		assert((i & ~15) * n == r[i >> 4] << 4);
	}
	for(unsigned i=0; i<256; i++)
	{
		assert((i &  ~0) * n == r[i]);
		assert((i &  ~1) * n == r[i >> 1] << 1);
		assert((i &  ~3) * n == r[i >> 2] << 2);
		assert((i &  ~7) * n == r[i >> 3] << 3);
		assert((i & ~15) * n == r[i >> 4] << 4);
	}
}

int main(void)
{
	checkmul<3, 170>();
	checkmul<4, 128>();
	checkmul<5, 204>();
	checkmul<6, 170>();
	checkmul<7, 128>();
	checkmul<8,  64>();
	checkmul<9, 170>();
}
