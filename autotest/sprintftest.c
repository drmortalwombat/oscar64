#include <stdio.h>
#include <string.h>
#include <assert.h>

void testi(const char * fmt, int val, const char * tst)
{
	char	buffer[32];

	sprintf(buffer, fmt, val);
	printf("%s:%s\n", buffer, tst);
	assert(!strcmp(buffer, tst));
}

void testu(const char * fmt, unsigned val, const char * tst)
{
	char	buffer[32];

	sprintf(buffer, fmt, val);
	printf("%s:%s\n", buffer, tst);
	assert(!strcmp(buffer, tst));
}

void testil(const char * fmt, long val, const char * tst)
{
	char	buffer[32];

	sprintf(buffer, fmt, val);
	printf("%s:%s\n", buffer, tst);
	assert(!strcmp(buffer, tst));
}

void testul(const char * fmt, unsigned long val, const char * tst)
{
	char	buffer[32];

	sprintf(buffer, fmt, val);
	printf("%s:%s\n", buffer, tst);
	assert(!strcmp(buffer, tst));
}

void testf(const char * fmt, float val, const char * tst)
{
	char	buffer[32];

	sprintf(buffer, fmt, val);
	printf("%s:%s\n", buffer, tst);
	assert(!strcmp(buffer, tst));
}


int main(void)
{
	testi("%d",  0, "0");
	testi("%d",  1, "1");
	testi("%d", -1, "-1");
	testi("%d", 12, "12");
	testi("%d", 123, "123");
	testi("%d", 1234, "1234");
	testi("%d", 12345, "12345");
	testi("%d", -12345, "-12345");
	testi("%d", 32767, "32767");
	testi("%d", -32768, "-32768");

	testi("%3d",  0, "  0");
	testi("%3d",  1, "  1");
	testi("%3d", -1, " -1");
	testi("%3d", 12, " 12");
	testi("%3d", 123, "123");
	testi("%3d", 1234, "1234");
	testi("%3d", 12345, "12345");
	testi("%3d", -12345, "-12345");
	testi("%3d", 32767, "32767");
	testi("%3d", -32768, "-32768");

	testi("%03d",  0, "000");
	testi("%03d",  1, "001");
//	testi("%03d", -1, "-01");
	testi("%03d", 12, "012");
	testi("%03d", 123, "123");
	testi("%03d", 1234, "1234");
	testi("%03d", 12345, "12345");
	testi("%03d", -12345, "-12345");
	testi("%03d", 32767, "32767");
	testi("%03d", -32768, "-32768");

	testi("%-4d",  0, "0   ");
	testi("%-4d",  1, "1   ");
	testi("%-4d", -1, "-1  ");
	testi("%-4d", 12, "12  ");
	testi("%-4d", 123, "123 ");
	testi("%-4d", 1234, "1234");
	testi("%-4d", 12345, "12345");
	testi("%-4d", -12345, "-12345");
	testi("%-4d", 32767, "32767");
	testi("%-4d", -32768, "-32768");

	testi("%+d",  0, "+0");
	testi("%+d",  1, "+1");
	testi("%+d", -1, "-1");
	testi("%+d", 12, "+12");
	testi("%+d", 123, "+123");
	testi("%+d", 1234, "+1234");
	testi("%+d", 12345, "+12345");
	testi("%+d", -12345, "-12345");
	testi("%+d", 32767, "+32767");
	testi("%+d", -32768, "-32768");



	testil("%ld",  0l, "0");
	testil("%ld",  1l, "1");
	testil("%ld", -1l, "-1");
	testil("%ld", 12l, "12");
	testil("%ld", 123l, "123");
	testil("%ld", 1234l, "1234");
	testil("%ld", 12345l, "12345");
	testil("%ld", -12345l, "-12345");
	testil("%ld", 32767l, "32767");
	testil("%ld", -32768l, "-32768");
	testil("%ld", 2147483647l, "2147483647");
	testil("%ld", -2147483648l, "-2147483648");

	testil("%3ld",  0l, "  0");
	testil("%3ld",  1l, "  1");
	testil("%3ld", -1l, " -1");
	testil("%3ld", 12l, " 12");
	testil("%3ld", 123l, "123");
	testil("%3ld", 1234l, "1234");
	testil("%3ld", 12345l, "12345");
	testil("%3ld", -12345l, "-12345");
	testil("%3ld", 32767l, "32767");
	testil("%3ld", -32768l, "-32768");
	testil("%3ld", 2147483647l, "2147483647");
	testil("%3ld", -2147483648l, "-2147483648");

	testil("%03ld",  0l, "000");
	testil("%03ld",  1l, "001");
//	testil("%03ld", -1l, "-01");
	testil("%03ld", 12l, "012");
	testil("%03ld", 123l, "123");
	testil("%03ld", 1234l, "1234");
	testil("%03ld", 12345l, "12345");
	testil("%03ld", -12345l, "-12345");
	testil("%03ld", 32767l, "32767");
	testil("%03ld", -32768l, "-32768");
	testil("%03ld", 2147483647l, "2147483647");
	testil("%03ld", -2147483648l, "-2147483648");

	testil("%-4ld",  0l, "0   ");
	testil("%-4ld",  1l, "1   ");
	testil("%-4ld", -1l, "-1  ");
	testil("%-4ld", 12l, "12  ");
	testil("%-4ld", 123l, "123 ");
	testil("%-4ld", 1234l, "1234");
	testil("%-4ld", 12345l, "12345");
	testil("%-4ld", -12345l, "-12345");
	testil("%-4ld", 32767l, "32767");
	testil("%-4ld", -32768l, "-32768");
	testil("%-4ld", 2147483647l, "2147483647");
	testil("%-4ld", -2147483648l, "-2147483648");

	testil("%+ld",  0l, "+0");
	testil("%+ld",  1l, "+1");
	testil("%+ld", -1l, "-1");
	testil("%+ld", 12l, "+12");
	testil("%+ld", 123l, "+123");
	testil("%+ld", 1234l, "+1234");
	testil("%+ld", 12345l, "+12345");
	testil("%+ld", -12345l, "-12345");
	testil("%+ld", 32767l, "+32767");
	testil("%+ld", -32768l, "-32768");
	testil("%+ld", 2147483647l, "+2147483647");
	testil("%+ld", -2147483648l, "-2147483648");

	testu("%u",  0, "0");
	testu("%u",  1, "1");
	testu("%u", 12, "12");
	testu("%u", 123, "123");
	testu("%u", 1234, "1234");
	testu("%u", 12345, "12345");
	testu("%u", 32767, "32767");
	testu("%u", 32768, "32768");
	testu("%u", 65535, "65535");
	testu("%x", 0, "0");
	testu("%x", 0x49bf, "49BF");
	testu("%x", 0xffff, "FFFF");

	testu("%3u",  0, "  0");
	testu("%3u",  1, "  1");
	testu("%3u", 12, " 12");
	testu("%3u", 123, "123");
	testu("%3u", 1234, "1234");
	testu("%3u", 12345, "12345");
	testu("%3u", 32767, "32767");
	testu("%3u", 32768, "32768");
	testu("%3u", 65535, "65535");
	testu("%3x", 0, "  0");
	testu("%3x", 0x49bf, "49BF");
	testu("%3x", 0xffff, "FFFF");

	testu("%03u",  0, "000");
	testu("%03u",  1, "001");
	testu("%03u", 12, "012");
	testu("%03u", 123, "123");
	testu("%03u", 1234, "1234");
	testu("%03u", 12345, "12345");
	testu("%03u", 32767, "32767");
	testu("%03u", 32768, "32768");
	testu("%03u", 65535, "65535");
	testu("%03x", 0, "000");
	testu("%03x", 0x49bf, "49BF");
	testu("%03x", 0xffff, "FFFF");

	testu("%-4u",  0, "0   ");
	testu("%-4u",  1, "1   ");
	testu("%-4u", 12, "12  ");
	testu("%-4u", 123, "123 ");
	testu("%-4u", 1234, "1234");
	testu("%-4u", 12345, "12345");
	testu("%-4u", 32767, "32767");
	testu("%-4u", 32768, "32768");
	testu("%-4u", 65535, "65535");
	testu("%-4x", 0, "0   ");
	testu("%-4x", 0x49bf, "49BF");
	testu("%-4x", 0xffff, "FFFF");

	testul("%3lu",  0l, "  0");
	testul("%3lu",  1l, "  1");
	testul("%3lu", 12l, " 12");
	testul("%3lu", 123l, "123");
	testul("%3lu", 1234l, "1234");
	testul("%3lu", 12345l, "12345");
	testul("%3lu", 32767l, "32767");
	testul("%3lu", 2147483647l, "2147483647");
	testul("%3lu", 4294967295l, "4294967295");
	testul("%3lx", 0, "  0");
	testul("%3lx", 0x3576fbcdl, "3576FBCD");
	testul("%3lx", 0xffffffffl, "FFFFFFFF");

	testul("%03lu",  0l, "000");
	testul("%03lu",  1l, "001");
	testul("%03lu", 12l, "012");
	testul("%03lu", 123l, "123");
	testul("%03lu", 1234l, "1234");
	testul("%03lu", 12345l, "12345");
	testul("%03lu", 32767l, "32767");
	testul("%03lu", 2147483647l, "2147483647");
	testul("%03lu", 4294967295l, "4294967295");
	testul("%03lx", 0, "000");
	testul("%03lx", 0x3576fbcdl, "3576FBCD");
	testul("%03lx", 0xffffffffl, "FFFFFFFF");

	testul("%-4lu",  0l, "0   ");
	testul("%-4lu",  1l, "1   ");
	testul("%-4lu", 12l, "12  ");
	testul("%-4lu", 123l, "123 ");
	testul("%-4lu", 1234l, "1234");
	testul("%-4lu", 12345l, "12345");
	testul("%-4lu", 32767l, "32767");
	testul("%-4lu", 2147483647l, "2147483647");
	testul("%-4lu", 4294967295l, "4294967295");
	testul("%-4lx", 0, "0   ");
	testul("%-4lx", 0x3576fbcdl, "3576FBCD");
	testul("%-4lx", 0xffffffffl, "FFFFFFFF");

	testul("%+lu",  0l, "+0");
	testul("%+lu",  1l, "+1");
	testul("%+lu", 12l, "+12");
	testul("%+lu", 123l, "+123");
	testul("%+lu", 1234l, "+1234");
	testul("%+lu", 12345l, "+12345");
	testul("%+lu", 32767l, "+32767");
	testul("%+lu", 2147483647l, "+2147483647");
	testul("%+lu", 4294967295l, "+4294967295");

	testf("%f",  0., "0.000000");
	testf("%f",  1., "1.000000");
	testf("%f", -1., "-1.000000");
	testf("%f", 12., "12.000000");
	testf("%f", 123., "123.000000");
	testf("%f", 1234., "1234.000000");
	testf("%f", 12345., "12345.000000");
	testf("%f", 123456., "123456.000000");
	testf("%f", 1234567., "1234567.000000");
	testf("%f",  0.1, "0.100000");
	testf("%f",  0.01, "0.010000");
	testf("%f",  0.001, "0.001000");
	testf("%f",  0.0001, "0.000100");
	testf("%f",  0.00001, "0.000010");
	testf("%f",  0.000001, "0.000001");

	testf("%5.1f",  0, "  0.0");
	testf("%5.1f",  1, "  1.0");
	testf("%5.1f", -1, " -1.0");
	testf("%5.1f",  10, " 10.0");
	testf("%5.1f", -10, "-10.0");
	testf("%5.1f",  100, "100.0");
	testf("%5.1f", -100, "-100.0");
	testf("%5.1f",  0.1, "  0.1");
	testf("%5.1f", -0.1, " -0.1");
	testf("%5.1f",  0.04, "  0.0");
	testf("%5.1f", -0.04, " -0.0");
	testf("%5.1f",  0.051, "  0.1");
	testf("%5.1f", -0.051, " -0.1");

	testf("%+5.1f",  0, " +0.0");
	testf("%+5.1f",  1, " +1.0");
	testf("%+5.1f", -1, " -1.0");
	testf("%+5.1f",  10, "+10.0");
	testf("%+5.1f", -10, "-10.0");
	testf("%+5.1f",  100, "+100.0");
	testf("%+5.1f", -100, "-100.0");
	testf("%+5.1f",  0.1, " +0.1");
	testf("%+5.1f", -0.1, " -0.1");
	testf("%+5.1f",  0.04, " +0.0");
	testf("%+5.1f", -0.04, " -0.0");
	testf("%+5.1f",  0.051, " +0.1");
	testf("%+5.1f", -0.051, " -0.1");

	return 0;
}