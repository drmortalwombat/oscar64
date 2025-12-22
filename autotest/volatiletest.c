#include <assert.h>
#include <autotest.h>

inline char udiff(char a, char b)
{
	return b - a;
}

void test_readorder(void)
{
	char	u0 = testio.cnt0;
	char	u1 = testio.cnt0;
	char	t = testio.cnt0;
	char	u2 = testio.cnt0;

	t = testio.cnt0;
	t = testio.cnt0;
	t = testio.cnt0;
	char	u3 = testio.cnt0;
	for(char i=0; i<16; i++)
		t = testio.cnt0;
	char	u4 = testio.cnt0;

	assert(udiff(u0, u1) == 1);
	assert(udiff(u1, u2) == 2);
	assert(udiff(u2, u3) == 4);
	assert(udiff(u3, u4) == 17);
}

void test_writeorder(void)
{
	char	u0 = testio.cnt0;
	char	u1 = testio.cnt0;
	testio.cnt0 = 0;
	char	u2 = testio.cnt0;

	testio.cnt0 = 0;
	testio.cnt0 = 0;
	testio.cnt0 = 0;
	char	u3 = testio.cnt0;
	for(char i=0; i<16; i++)
		testio.cnt0 = 0;
	char	u4 = testio.cnt0;

	assert(udiff(u0, u1) == 1);
	assert(udiff(u1, u2) == 2);
	assert(udiff(u2, u3) == 4);
	assert(udiff(u3, u4) == 17);	
}

volatile char buffer[16];

void test_dma(void)
{
	testio.dmaddr = unsigned(buffer);
	testio.dmdata = 0;
	testio.dmdata = 1;
	testio.dmdata = 2;
	testio.dmdata = 3;
	testio.dmdata = 0;
	testio.dmdata = 0;
	testio.dmdata = 0;
	testio.dmdata = 0;
	testio.dmdata = 4;
	testio.dmdata = 5;
	testio.dmdata = 6;
	testio.dmdata = 7;
	testio.dmdata = 0;
	testio.dmdata = 1;
	testio.dmdata = 2;
	testio.dmdata = 3;
	assert(buffer[ 0] == 0);
	assert(buffer[ 1] == 1);
	assert(buffer[ 2] == 2);
	assert(buffer[ 3] == 3);
	assert(buffer[ 4] == 0);
	assert(buffer[ 5] == 0);
	assert(buffer[ 6] == 0);
	assert(buffer[ 7] == 0);
	assert(buffer[ 8] == 4);
	assert(buffer[ 9] == 5);
	assert(buffer[10] == 6);
	assert(buffer[11] == 7);
	assert(buffer[12] == 0);
	assert(buffer[13] == 1);
	assert(buffer[14] == 2);
	assert(buffer[15] == 3);
	testio.dmaddr = unsigned(buffer);
	char tbuffer[16];
	for(char i=0; i<16; i++)
		tbuffer[i] = testio.dmdata;
	assert(tbuffer[ 0] == 0);
	assert(tbuffer[ 1] == 1);
	assert(tbuffer[ 2] == 2);
	assert(tbuffer[ 3] == 3);
	assert(tbuffer[ 4] == 0);
	assert(tbuffer[ 5] == 0);
	assert(tbuffer[ 6] == 0);
	assert(tbuffer[ 7] == 0);
	assert(tbuffer[ 8] == 4);
	assert(tbuffer[ 9] == 5);
	assert(tbuffer[10] == 6);
	assert(tbuffer[11] == 7);
	assert(tbuffer[12] == 0);
	assert(tbuffer[13] == 1);
	assert(tbuffer[14] == 2);
	assert(tbuffer[15] == 3);
}

char read_cnt0(void)
{
	return testio.cnt0;
}

void test_unusedread(void)
{
	char base0 = testio.cnt0;
	read_cnt0();
	char end0 = testio.cnt0;

	assert(udiff(base0, end0) == 2);
}

void test_readsmallfunc(void)
{
	char base0 = testio.cnt0;
	char u0 = read_cnt0();
	char u1 = read_cnt0();
	char end0 = testio.cnt0;

	assert(udiff(base0, u0) == 1);
	assert(udiff(u0, u1) == 1);
	assert(udiff(u1, end0) == 1);
}

int main(void)
{
	test_readorder();
	test_writeorder();
	test_dma();
	test_unusedread();
	test_readsmallfunc();
}
