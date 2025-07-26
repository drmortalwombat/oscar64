#include <assert.h>
#include <stdio.h>
#include <string.h>

char dest[2000], src[2000];

int main(void)
{
	// Test strcat with basic strings
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "hello");
	strcat(dest, " world");
	assert(strcmp(dest, "hello world") == 0);

	// Test strcat with empty string
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "test");
	strcat(dest, "");
	assert(strcmp(dest, "test") == 0);

	// Test strcat to empty string
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "");
	strcat(dest, "append");
	assert(strcmp(dest, "append") == 0);

	// Test strcat multiple concatenations
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "a");
	strcat(dest, "b");
	strcat(dest, "c");
	strcat(dest, "d");
	assert(strcmp(dest, "abcd") == 0);

	// Test strcat with single characters
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "x");
	strcat(dest, "y");
	assert(strcmp(dest, "xy") == 0);

	// Test strcat with longer strings
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "beginning");
	strcat(dest, "_middle_");
	strcat(dest, "end");
	assert(strcmp(dest, "beginning_middle_end") == 0);

	// Test strncat with basic functionality
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "hello");
	strncat(dest, " world", 6);
	assert(memcmp(dest, "hello world\0#", 13) == 0);
	
	// Test strncat with partial concatenation
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "hello");
	strncat(dest, " world", 3);
	assert(memcmp(dest, "hello wo\0#", 10) == 0);

	// Test strncat with zero length
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "original");
	strncat(dest, "ignored", 0);
	assert(memcmp(dest, "original\0#", 10) == 0);

	// Test strncat with exact length
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "test");
	strncat(dest, "1234", 4);
	assert(memcmp(dest, "test1234\0#", 10) == 0);

	// Test strncat where n is larger than source
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "base");
	strncat(dest, "add", 10);
	assert(memcmp(dest, "baseadd\0#", 9) == 0);

	// Test strncat with empty source
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "keep");
	strncat(dest, "", 5);
	assert(memcmp(dest, "keep\0#", 6) == 0);

	// Test strncat with empty destination
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "");
	strncat(dest, "new", 3);
	assert(memcmp(dest, "new\0#", 4) == 0);

	// Test strncat with single character
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "a");
	strncat(dest, "bcdef", 1);
	assert(memcmp(dest, "ab\0#", 4) == 0);

	// Test strncat with long strings
	memset(dest, '#', sizeof(dest));
	for (int i = 0; i < 500; i++)
	{
		src[i] = 'a' + (i & 7);
	}
	src[500] = '\0';

	strcpy(dest, "prefix_");
	strcat(dest, src);
	assert(strlen(dest) == 507); // 7 + 500
	assert(strncmp(dest, "prefix_", 7) == 0);
	assert(memcmp(dest + 7, src, 500) == 0);

	// Test strncat with long strings
	memset(dest, '#', sizeof(dest));
	strcpy(dest, "start_");
	strncat(dest, src, 100);
	assert(strlen(dest) == 106); // 6 + 100
	assert(strncmp(dest, "start_", 6) == 0);
	assert(strncmp(dest + 6, src, 100) == 0); 
	assert(dest[106] == '\0'); // ensure null termination
	
	// Test strncat with partial long string
	memset(dest, '#', sizeof(dest));
	for (int i = 0; i < 1000; i++)
	{
		src[i] = 'x' + (i & 3);
	}
	src[1000] = '\0';

	strcpy(dest, "begin");
	strncat(dest, src, 50);
	assert(strlen(dest) == 55); // 5 + 50
	assert(strncmp(dest, "begin", 5) == 0);
	assert(strncmp(dest + 5, src, 50) == 0);
	assert(dest[55] == '\0'); // ensure null termination

	return 0;
}
