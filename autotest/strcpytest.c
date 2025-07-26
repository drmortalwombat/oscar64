#include <assert.h>
#include <stdio.h>
#include <string.h>

char dest[1025], src[1025];

int main(void)
{
	strcpy(dest, "hello");
	assert(strcmp(dest, "hello") == 0);

	strcpy(dest, "");
	assert(strcmp(dest, "") == 0);

	strcpy(dest, "a");
	assert(strcmp(dest, "a") == 0);

	strcpy(dest, "abcdefghijklmnopqrstuvwxyz");
	assert(strcmp(dest, "abcdefghijklmnopqrstuvwxyz") == 0);

	// Test strcpy with long string
	for (int i = 0; i < 1024; i++)
	{
		src[i] = 'a' + (i & 7);
	}
	src[1024] = 0;

	strcpy(dest, src);
	assert(strcmp(dest, src) == 0);

	// Test strncpy with various lengths
	strcpy(dest, "xxxxxxxxxx");
	strncpy(dest, "hello", 5);
	dest[5] = 0; // null terminate for comparison
	assert(strcmp(dest, "hello") == 0);

	strcpy(dest, "xxxxxxxxxx");
	strncpy(dest, "hello", 3);
	dest[3] = 0; // null terminate for comparison
	assert(strcmp(dest, "hel") == 0);

	strcpy(dest, "xxxxxxxxxx");
	strncpy(dest, "hi", 5);
	assert(dest[0] == 'h');
	assert(dest[1] == 'i');
	assert(dest[2] == 0);
	assert(dest[3] == 0);
	assert(dest[4] == 0);

	// Test strncpy with exact length
	strcpy(dest, "xxxxxxxxxx");
	strncpy(dest, "test", 4);
	dest[4] = 0; // null terminate for comparison
	assert(strcmp(dest, "test") == 0);

	// Test strncpy with zero length
	strcpy(dest, "original");
	strncpy(dest, "new", 0);
	assert(strcmp(dest, "original") == 0);

	// Test strncpy copying longer than source
	strcpy(dest, "xxxxxxxxxx");
	strncpy(dest, "ab", 8);
	assert(dest[0] == 'a');
	assert(dest[1] == 'b');
	assert(dest[2] == 0);
	assert(dest[3] == 0);
	assert(dest[4] == 0);
	assert(dest[5] == 0);
	assert(dest[6] == 0);
	assert(dest[7] == 0);

	// Test strncpy with large string
	strcpy(dest, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	strncpy(dest, src, 1000);
	dest[1000] = 0; // ensure null termination
	src[1000] = 0;	// truncate source for comparison
	assert(strcmp(dest, src) == 0);

	return 0;
}
