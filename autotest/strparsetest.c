#include <stdlib.h>
#include <assert.h>

void check_strtol(const char * str, char base, long val, char term)
{
	char * t;
	assert(val == strtol(str, &t, base));
	assert(*t == term);	
}

void check_strtoul(const char * str, char base, unsigned long val, char term)
{
	char * t;
	assert(val == strtoul(str, &t, base));
	assert(*t == term);	
}

void check_strtoi(const char * str, char base, int val, char term)
{
	char * t;
	assert(val == strtoi(str, &t, base));
	assert(*t == term);	
}

void check_strtou(const char * str, char base, unsigned val, char term)
{
	char * t;
	assert(val == strtou(str, &t, base));
	assert(*t == term);	
}

void check_strtof(const char * str, float val, char term)
{
	char * t;
	assert(val == strtof(str, &t));
	assert(*t == term);	
}

int main(void)
{
	check_strtol("123", 10, 123, '\0');
	check_strtol("123x", 10, 123, 'x');
	check_strtol(" 123x", 10, 123, 'x');
	check_strtol("-123x", 10, -123, 'x');

	check_strtoul("123", 10, 123, '\0');
	check_strtoul("123x", 10, 123, 'x');
	check_strtoul(" 123x", 10, 123, 'x');

	check_strtoi("123", 10, 123, '\0');
	check_strtoi("123x", 10, 123, 'x');
	check_strtoi(" 123x", 10, 123, 'x');
	check_strtoi("-123x", 10, -123, 'x');

	check_strtou("123", 10, 123, '\0');
	check_strtou("123x", 10, 123, 'x');
	check_strtou(" 123x", 10, 123, 'x');
	check_strtof("123", 123, '\0');
	check_strtof("123x", 123, 'x');
	check_strtof(" 123x", 123, 'x');
	check_strtof("-123x", -123, 'x');
	check_strtof("123.5", 123.5, '\0');
	check_strtof("123.5x", 123.5, 'x');
	check_strtof(" 123.5x", 123.5, 'x');
	check_strtof("-123.5x", -123.5, 'x');
	check_strtof("123e2", 12300, '\0');
	check_strtof("123e2x", 12300, 'x');
	check_strtof(" 123e2x", 12300, 'x');
	check_strtof("-123e2x", -12300, 'x');
	check_strtof("123.5e2", 12350, '\0');
	check_strtof("123.5e2x", 12350, 'x');
	check_strtof(" 123.5e2x", 12350, 'x');
	check_strtof("-123.5e2x", -12350, 'x');
	return 0;
}