#include <assert.h>
#include <stdio.h>
#include <string.h>

char teststr[2000], substr[200];

int main(void)
{
	// Test strstr with basic functionality
	assert(strcmp(strstr("hello world", "hello"), "hello world") == 0);
	assert(strcmp(strstr("hello world", "world"), "world") == 0);
	assert(strcmp(strstr("hello world", "lo wo"), "lo world") == 0);
	assert(strstr("hello world", "xyz") == nullptr);
	
	// Test strstr with empty substring
	assert(strcmp(strstr("hello", ""), "hello") == 0);  // empty substring matches at start
	assert(strcmp(strstr("", ""), "") == 0);
	
	// Test strstr with substring not found
	assert(strstr("hello", "world") == nullptr);
	assert(strstr("abc", "xyz") == nullptr);
	assert(strstr("short", "longer") == nullptr);
	
	// Test strstr with single character substring
	assert(strcmp(strstr("hello", "h"), "hello") == 0);
	assert(strcmp(strstr("hello", "e"), "ello") == 0);
	assert(strcmp(strstr("hello", "o"), "o") == 0);
	assert(strstr("hello", "x") == nullptr);
	
	// Test strstr with substring at different positions
	assert(strcmp(strstr("abcdefgh", "abc"), "abcdefgh") == 0);  // at start
	assert(strcmp(strstr("abcdefgh", "def"), "defgh") == 0);     // in middle
	assert(strcmp(strstr("abcdefgh", "fgh"), "fgh") == 0);       // at end
	
	// Test strstr with repeated patterns
	assert(strcmp(strstr("ababab", "ab"), "ababab") == 0);       // finds first occurrence
	assert(strcmp(strstr("ababab", "ba"), "babab") == 0);
	assert(strcmp(strstr("ababab", "bab"), "babab") == 0);
	
	// Test strstr with overlapping patterns
	assert(strcmp(strstr("aaaa", "aa"), "aaaa") == 0);           // finds first match
	assert(strcmp(strstr("ababa", "aba"), "ababa") == 0);
	
	// Test strstr where substring equals the string
	assert(strcmp(strstr("test", "test"), "test") == 0);
	assert(strcmp(strstr("a", "a"), "a") == 0);
	
	// Test strstr with special characters
	assert(strcmp(strstr("hello, world!", "o, w"), "o, world!") == 0);
	assert(strcmp(strstr("a.b.c.d", ".b."), ".b.c.d") == 0);
	assert(strcmp(strstr("x y z", " y "), " y z") == 0);
	
	// Build a long test string with pattern
	for(int i = 0; i < 1900; i++)
	{
		teststr[i] = 'a' + (i & 7);
	}
	teststr[1900] = '\0';
	
	// Test strstr with long strings
	strcpy(substr, "abcdefgh");
	char *result = strstr(teststr, substr);
	assert(result == teststr);  // pattern starts at beginning
	assert(strncmp(result, substr, 8) == 0);
	
	// Test strstr with pattern that appears later
	strcpy(substr, "bcdefgha");
	result = strstr(teststr, substr);
	assert(result == teststr + 1);  // pattern starts at position 1
	assert(strncmp(result, substr, 8) == 0);
	
	// Test strstr with pattern not in string
	strcpy(substr, "zzzzz");
	assert(strstr(teststr, substr) == nullptr);
	
	// Test strstr with longer substring than string
	strcpy(teststr, "short");
	strcpy(substr, "this is longer");
	assert(strstr(teststr, substr) == nullptr);
	
	// Test strstr with case sensitivity
	assert(strstr("Hello", "hello") == nullptr);  // case sensitive
	assert(strcmp(strstr("Hello", "Hello"), "Hello") == 0);
	
	// Test strstr with partial matches that fail
	assert(strstr("abcde", "abcx") == nullptr);
	assert(strstr("testing", "tester") == nullptr);
	
	// Test strstr with multiple occurrences (finds first)
	strcpy(teststr, "the quick brown fox jumps over the lazy dog");
	assert(strcmp(strstr(teststr, "the"), "the quick brown fox jumps over the lazy dog") == 0);
	assert(strcmp(strstr(teststr, "o"), "own fox jumps over the lazy dog") == 0);  // first 'o'
	
	// Test strstr with whole word searches
	assert(strcmp(strstr(teststr, "quick"), "quick brown fox jumps over the lazy dog") == 0);
	assert(strcmp(strstr(teststr, "fox"), "fox jumps over the lazy dog") == 0);
	assert(strcmp(strstr(teststr, "dog"), "dog") == 0);
	
	return 0;
}
