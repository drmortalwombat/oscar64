#ifndef AUTOTEST_H
#define AUTOTEST_H

struct TestIO
{
	volatile char					cnt0, cnt1;
	volatile __memmap	unsigned	dmaddr;
	volatile __memmap	char		dmdata;
	volatile char					mirror0, mirror1, mirror2;
	volatile long					cycles;
	volatile char 					scratch[4];
};

#define testio	(*((struct TestIO *)0xdd80))
#define testio2	(*((struct TestIO *)0xdd90))

#endif
