#include <assert.h>
#include <stdio.h>
#include <string.h>

char teststr[2000];

int main(void)
{
	// Test strchr with basic functionality
	assert(strcmp(strchr("hello", 'h'), "hello") == 0);
	assert(strcmp(strchr("hello", 'e'), "ello") == 0);
	assert(strcmp(strchr("hello", 'l'), "llo") == 0);  // finds first 'l'
	assert(strcmp(strchr("hello", 'o'), "o") == 0);
	assert(strchr("hello", 'x') == nullptr);
	
	// Test strchr with null character
	strcpy(teststr, "hello");
	assert(strchr(teststr, '\0') == teststr + 5); // should point to end of string
	
	// Test strchr with empty string
	assert(strchr("", 'a') == nullptr);
	strcpy(teststr, "");
	assert(strchr(teststr, '\0') == teststr); // should point to start of
	
	// Test strchr with single character string
	assert(strcmp(strchr("a", 'a'), "a") == 0);
	assert(strchr("a", 'b') == nullptr);
	strcpy(teststr, "a");
	assert(strchr(teststr, '\0') == teststr + 1); // should point to end of single character string
	
	// Test strchr with repeated characters
	assert(strcmp(strchr("aaaaaa", 'a'), "aaaaaa") == 0);  // finds first occurrence
	assert(strcmp(strchr("abcabc", 'a'), "abcabc") == 0);  // finds first 'a'
	assert(strcmp(strchr("abcabc", 'b'), "bcabc") == 0);   // finds first 'b'
	assert(strcmp(strchr("abcabc", 'c'), "cabc") == 0);    // finds first 'c'
	
	// Test strchr with special characters
	assert(strcmp(strchr("hello world", ' '), " world") == 0);
	assert(strcmp(strchr("a.b,c;d", '.'), ".b,c;d") == 0);
	assert(strcmp(strchr("a.b,c;d", ','), ",c;d") == 0);
	assert(strcmp(strchr("a.b,c;d", ';'), ";d") == 0);
	
	// Test strrchr with basic functionality
	assert(strcmp(strrchr("hello", 'h'), "hello") == 0);
	assert(strcmp(strrchr("hello", 'e'), "ello") == 0);
	assert(strcmp(strrchr("hello", 'l'), "lo") == 0);    // finds last 'l'
	assert(strcmp(strrchr("hello", 'o'), "o") == 0);
	assert(strrchr("hello", 'x') == nullptr);
	
	// Test strrchr with null character
	strcpy(teststr, "hello");
	assert(strrchr(teststr, '\0') == teststr + 5); // should point to end of string
	
	// Test strrchr with empty string
	assert(strrchr("", 'a') == nullptr);
	strcpy(teststr, "");
	assert(strrchr(teststr, '\0') == teststr); // should point to start
	
	// Test strrchr with single character string
	assert(strcmp(strrchr("a", 'a'), "a") == 0);
	assert(strrchr("a", 'b') == nullptr);	// Test strrchr with repeated characters
	assert(strcmp(strrchr("aaaaaa", 'a'), "a") == 0);      // finds last 'a'
	assert(strcmp(strrchr("abcabc", 'a'), "abc") == 0);    // finds last 'a'
	assert(strcmp(strrchr("abcabc", 'b'), "bc") == 0);     // finds last 'b'
	assert(strcmp(strrchr("abcabc", 'c'), "c") == 0);      // finds last 'c'
	
	// Test strrchr with special characters
	assert(strcmp(strrchr("hello world test", ' '), " test") == 0);
	assert(strcmp(strrchr("a.b.c.d", '.'), ".d") == 0);
	assert(strcmp(strrchr("a,b,c,d", ','), ",d") == 0);
	
	// Test with longer strings
	strcpy(teststr, "this is a test string with multiple a characters");
	assert(strchr(teststr, 'a') == teststr + 8);   // first 'a' at position 8
	assert(strrchr(teststr, 'a') == teststr + 42);  // last 'a' at position 42
	
	// Test with character not found in long string
	assert(strchr(teststr, 'z') == nullptr);
	assert(strrchr(teststr, 'z') == nullptr);
	
	// Test with first and last character
	strcpy(teststr, "abcdefghijklmnopqrstuvwxyza");
	assert(strchr(teststr, 'a') == teststr);        // first character
	assert(strrchr(teststr, 'a') == teststr + 26);  // last character
	
	// Build a long test string with pattern
	for(int i = 0; i < 1000; i++)
	{
		teststr[i] = 'a' + (i % 26);
	}
	teststr[1000] = '\0';
	
	// Test strchr and strrchr with pattern
	assert(strchr(teststr, 'a') == teststr);        // first 'a' at start
	assert(strchr(teststr, 'z') == teststr + 25);   // first 'z' at position 25
	assert(strrchr(teststr, 'a') == teststr + 988); // last 'a' at 988 (988 % 26 == 0)
	assert(strrchr(teststr, 'z') == teststr + 987); // last 'z' at 987 (987 % 26 == 25)
	
	// Test with character that appears only once
	strcpy(teststr, "abcdefghijklmnopqrstuvwxyz");
	for(char c = 'a'; c <= 'z'; c++)
	{
		char *first = strchr(teststr, c);
		char *last = strrchr(teststr, c);
		assert(first == last);  // should be same for unique characters
		assert(first != nullptr);
		assert(*first == c);
	}
	
	return 0;
}
