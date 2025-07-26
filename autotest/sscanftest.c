#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define EPSILON 0.0001

int scan_i(const char * fmt, const char * tst) {
	int val;

	sscanf(tst, fmt, &val);
	printf(fmt, val);
	puts("\n");

	return val;
}

long scan_l(const char * fmt, const char * tst) {
	long val;

	sscanf(tst, fmt, &val);
	printf(fmt, val);
	puts("\n");

	return val;
}

unsigned int scan_ui(const char * fmt, const char * tst) {
	unsigned int val;

	sscanf(tst, fmt, &val);
	printf(fmt, val);
	puts("\n");

	return val;
}

unsigned long scan_ul(const char * fmt, const char * tst) {
	unsigned long val;

	sscanf(tst, fmt, &val);
	printf(fmt, val);
	puts("\n");

	return val;
}

float scan_f(const char * fmt, const char * tst) {
	float val;

	sscanf(tst, fmt, &val);
	printf(fmt, val);
	puts("\n");

	return val;
}

bool closeTo(float a, float b) {
	while (a < -1.0 || a > 1.0) a /= 10.0;
	while (b < -1.0 || b > 1.0) b /= 10.0;

	return fabs(a - b) < EPSILON;
}

int main(void)
{
	assert(scan_i("%d", "0") == 0);
	assert(scan_i("%d", "1") == 1);
	assert(scan_i("%d", "-1") == -1);
	assert(scan_i("%d", "12") == 12);
	assert(scan_i("%d", "123") == 123);
	assert(scan_i("%d", "1234") == 1234);
	assert(scan_i("%d", "12345") == 12345);
	assert(scan_i("%d", "-12345") == -12345);
	assert(scan_i("%d", "32767") == 32767);
	assert(scan_i("%d", "-32768") == -32768);

	assert(scan_l("%ld", "0") == 0l);
	assert(scan_l("%ld", "1") == 1l);
	assert(scan_l("%ld", "-1") == -1l);
	assert(scan_l("%ld", "12") == 12l);
	assert(scan_l("%ld", "123") == 123l);
	assert(scan_l("%ld", "1234") == 1234l);
	assert(scan_l("%ld", "12345") == 12345l);
	assert(scan_l("%ld", "-12345") == -12345l);
	assert(scan_l("%ld", "32767") == 32767l);
	assert(scan_l("%ld", "-32768") == -32768l);
	assert(scan_l("%ld", "2147483647") == 2147483647l);
	assert(scan_l("%ld", "-2147483648") == -2147483648l);

	assert(scan_ui("%u", "0") == 0);
	assert(scan_ui("%u", "1") == 1);
	assert(scan_ui("%u", "12") == 12);
	assert(scan_ui("%u", "123") == 123);
	assert(scan_ui("%u", "1234") == 1234);
	assert(scan_ui("%u", "12345") == 12345);
	assert(scan_ui("%u", "32767") == 32767);
	assert(scan_ui("%u", "32768") == 32768);
	assert(scan_ui("%u", "65535") == 65535);
	assert(scan_ui("%x", "0") == 0);
	assert(scan_ui("%x", "49BF") == 0x49bf);
	assert(scan_ui("%x", "FFFF") == 0xffff);

	assert(scan_ul("%lu", "000") == 0l);
	assert(scan_ul("%lu", "001") == 1l);
	assert(scan_ul("%lu", "012") == 12l);
	assert(scan_ul("%lu", "123") == 123l);
	assert(scan_ul("%lu", "1234") == 1234l);
	assert(scan_ul("%lu", "12345") == 12345l);
	assert(scan_ul("%lu", "32767") == 32767l);
	assert(scan_ul("%lu", "2147483647") == 2147483647l);
	assert(scan_ul("%lu", "4294967295") == 4294967295l);
	assert(scan_ul("%lx", "000") == 0);
	assert(scan_ul("%lx", "3576FBCD") == 0x3576fbcdl);
	assert(scan_ul("%lx", "FFFFFFFF") == 0xffffffffl);

	assert(closeTo(scan_f("%f", "0.000000"), 0.));
	assert(closeTo(scan_f("%f", "1.000000"), 1.));
	assert(closeTo(scan_f("%f", "-1.000000"), -1.));
	assert(closeTo(scan_f("%f", "12.000000"), 12.));
	assert(closeTo(scan_f("%f", "123.000000"), 123.));
	assert(closeTo(scan_f("%f", "1234.000000"), 1234.));
	assert(closeTo(scan_f("%f", "12345.000000"), 12345.));
	assert(closeTo(scan_f("%f", "123456.000000"), 123456.));
	assert(closeTo(scan_f("%f", "1234567.000000"), 1234567.));
	assert(closeTo(scan_f("%f", "0.100000"), 0.1));
	assert(closeTo(scan_f("%f", "0.010000"), 0.01));
	assert(closeTo(scan_f("%f", "0.001000"), 0.001));
	assert(closeTo(scan_f("%f", "0.000100"), 0.0001));
	assert(closeTo(scan_f("%f", "0.000010"), 0.00001));
	assert(closeTo(scan_f("%f", "0.000001"), 0.000001));

	return 0;
}