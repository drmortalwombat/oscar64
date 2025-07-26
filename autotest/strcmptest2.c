#include <string.h>
#include <assert.h>

char	aa[2000], ba[2000];

int main(void)
{
	assert(strcmp("abcdefgh", "abcdefgh") == 0);
	assert(strcmp("abcdefgh", "abcdemgh") < 0);
	assert(strcmp("abcdefgh", "abcdefghi") < 0);
	assert(strcmp("abcdefghi", "abcdefgh") > 0);
	assert(strcmp("abcdemgh", "abcdefgh") > 0);

	for(int i=0; i<1900; i++)
	{
		aa[i] = 'a' + (i & 7);
	}
	aa[1900] = 0;
	
	strcpy(ba, aa);
	
	assert(strcmp(aa, ba) == 0);
	ba[1000] = 'z';
	assert(strcmp(aa, ba) < 0);
	assert(strcmp(ba, aa) > 0);
	
	// Test strncmp with basic comparisons
	assert(strncmp("abcdefgh", "abcdefgh", 8) == 0);
	assert(strncmp("abcdefgh", "abcdemgh", 8) < 0);
	assert(strncmp("abcdefgh", "abcdefghi", 8) == 0);  // only compare first 8 chars
	assert(strncmp("abcdefghi", "abcdefgh", 9) > 0);
	assert(strncmp("abcdemgh", "abcdefgh", 8) > 0);
	
	// Test strncmp with partial comparisons
	assert(strncmp("abcdefgh", "abcdemgh", 4) == 0);  // first 4 chars are same
	assert(strncmp("abcdefgh", "abcdemgh", 6) < 0);   // difference at 5th char
	assert(strncmp("hello", "help", 3) == 0);         // "hel" vs "hel"
	assert(strncmp("hello", "help", 4) < 0);          // "hell" vs "help"
	
	// Test strncmp with zero length
	assert(strncmp("different", "strings", 0) == 0);  // zero length always equal
	assert(strncmp("", "", 0) == 0);
	assert(strncmp("a", "b", 0) == 0);
	
	// Test strncmp with empty strings
	assert(strncmp("", "", 5) == 0);
	assert(strncmp("hello", "", 5) > 0);
	assert(strncmp("", "hello", 5) < 0);
	assert(strncmp("hello", "", 1) > 0);
	assert(strncmp("", "hello", 1) < 0);
	
	// Test strncmp with single characters
	assert(strncmp("a", "a", 1) == 0);
	assert(strncmp("a", "b", 1) < 0);
	assert(strncmp("b", "a", 1) > 0);
	
	// Test strncmp where one string is shorter
	assert(strncmp("abc", "abcdef", 3) == 0);
	assert(strncmp("abc", "abcdef", 6) < 0);  // null vs 'd'
	assert(strncmp("abcdef", "abc", 6) > 0);  // 'd' vs null
	
	// Test strncmp with long strings
	strcpy(ba, aa);  // restore ba to match aa
	assert(strncmp(aa, ba, 1900) == 0);
	ba[1000] = 'z';
	assert(strncmp(aa, ba, 1000) == 0);   // equal up to position 1000
	assert(strncmp(aa, ba, 1001) < 0);    // difference at position 1000
	assert(strncmp(ba, aa, 1001) > 0);    // difference at position 1000
	assert(strncmp(aa, ba, 999) == 0);    // equal before the difference
	
	// Test strncmp with length larger than string length
	strcpy(ba, "short");
	assert(strncmp("short", ba, 100) == 0);  // equal even with large n
	assert(strncmp("short", "shorx", 100) < 0);  // difference within string
	
	return 0;
}
