#include <assert.h>

struct A
{
	char	x : 4;
	char	y : 1;
	char	z : 3;
};

A a = {7, 1, 2};

void test_char_fit(void)
{
	assert(a.x == 7);
	assert(a.y == 1);
	assert(a.z == 2);
	assert(sizeof(A) == 1);

	for(int i=0; i<16; i++)
	{
		a.x = i;
		a.y = 0;
		a.z = 3;
		assert(a.x == i);
		assert(a.y == 0);
		assert(a.z == 3);		
	}
}

struct B
{
	char	x : 6;
	char	y : 6;
	char	z : 6;
	char	w : 6;
};

B 	b = {11, 22, 33, 44};

void test_char_cross(void)
{
	assert(b.x == 11);
	assert(b.y == 22);
	assert(b.z == 33);
	assert(b.w == 44);
	assert(sizeof(B) == 3);

	for(int i=0; i<64; i++)
	{
		b.x = i * 1;
		b.y = i * 3;
		b.z = i * 5;
		b.w = i * 7;
		assert(b.x == ((i * 1) & 0x3f));
		assert(b.y == ((i * 3) & 0x3f));
		assert(b.z == ((i * 5) & 0x3f));		
		assert(b.w == ((i * 7) & 0x3f));		
	}
}

struct C
{
	unsigned	x : 4;
	unsigned	y : 1;
	unsigned	z : 3;
};

C c = {7, 1, 2};

void test_word_fit(void)
{
	assert(c.x == 7);
	assert(c.y == 1);
	assert(c.z == 2);
	assert(sizeof(C) == 1);

	for(int i=0; i<16; i++)
	{
		c.x = i;
		c.y = 0;
		c.z = 3;
		assert(c.x == i);
		assert(c.y == 0);
		assert(c.z == 3);		
	}
}

struct D
{
	unsigned	x : 10;
	unsigned	y : 10;
	unsigned	z : 10;
	unsigned	w : 10;
};

D 	d = {111, 222, 333, 444};

void test_word_cross(void)
{
	assert(d.x == 111);
	assert(d.y == 222);
	assert(d.z == 333);
	assert(d.w == 444);
	assert(sizeof(D) == 5);

	for(int i=0; i<1024; i++)
	{
		d.x = i * 1;
		d.y = i * 3;
		d.z = i * 5;
		d.w = i * 7;
		assert(d.x == ((i * 1) & 0x3ff));
		assert(d.y == ((i * 3) & 0x3ff));
		assert(d.z == ((i * 5) & 0x3ff));		
		assert(d.w == ((i * 7) & 0x3ff));		
	}
}

struct E
{
	unsigned long	x : 4;
	unsigned long	y : 1;
	unsigned long	z : 3;
};

E e = {7, 1, 2};

void test_dword_fit(void)
{
	assert(e.x == 7);
	assert(e.y == 1);
	assert(e.z == 2);
	assert(sizeof(E) == 1);

	for(int i=0; i<16; i++)
	{
		e.x = i;
		e.y = 0;
		e.z = 3;
		assert(e.x == i);
		assert(e.y == 0);
		assert(e.z == 3);		
	}
}

struct F
{
	unsigned long	x : 20;
	unsigned long	y : 20;
	unsigned long	z : 20;
	unsigned long	w : 20;
};

F 	f = {111111UL, 222222UL, 333333UL, 444444UL};

void test_dword_cross(void)
{
	assert(f.x == 111111UL);
	assert(f.y == 222222UL);
	assert(f.z == 333333UL);
	assert(f.w == 444444UL);
	assert(sizeof(F) == 10);

	for(int i=0; i<1024; i++)
	{
		f.x = i * 11UL;
		f.y = i * 33UL;
		f.z = i * 55UL;
		f.w = i * 77UL;
		assert(f.x == ((i * 11UL) & 0xfffffUL));
		assert(f.y == ((i * 33UL) & 0xfffffUL));
		assert(f.z == ((i * 55UL) & 0xfffffUL));		
		assert(f.w == ((i * 77UL) & 0xfffffUL));		
	}
}

struct G
{
	signed char x : 1;
	signed char y : 5;
	signed char z : 2;
};

G 	g = {0, -1, -2};

void test_char_signed(void)
{
	assert(g.x ==  0);
	assert(g.y == -1);
	assert(g.z == -2);
	assert(sizeof(G) == 1);

	for(int i=-16; i<16; i++)
	{
		g.x = -1;
		g.y = i;
		g.z = 1;
		assert(g.x == -1);
		assert(g.y == i);
		assert(g.z == 1);		
	}
}

struct H
{
	int	x : 10;
	int	y : 10;
	int	z : 10;
	int	w : 10;
};

H 	h = {111, -222, -333, 444};

void test_word_signed(void)
{
	assert(h.x == 111);
	assert(h.y == -222);
	assert(h.z == -333);
	assert(h.w == 444);
	assert(sizeof(H) == 5);

	for(int i=-32; i<32; i++)
	{
		h.x = i * 1;
		h.y = i * 3;
		h.z = i * 5;
		h.w = i * 7;
		assert(h.x == i * 1);
		assert(h.y == i * 3);
		assert(h.z == i * 5);		
		assert(h.w == i * 7);		
	}
}


int main(void)
{
	test_char_fit();
	test_char_cross();
	test_word_fit();
	test_word_cross();
	test_dword_fit();
	test_dword_cross();
	test_char_signed();
	test_word_signed();

	return 0;
}
