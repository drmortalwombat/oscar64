#include <assert.h>
#include <stdio.h>
#include <string.h>

char testmem[2000];

int main(void)
{
	// Test memchr with basic functionality
	assert(memcmp(memchr("hello", 'h', 5), "hello", 5) == 0);
	assert(memcmp(memchr("hello", 'e', 5), "ello", 4) == 0);
	assert(memcmp(memchr("hello", 'l', 5), "llo", 3) == 0);  // finds first 'l'
	assert(memcmp(memchr("hello", 'o', 5), "o", 1) == 0);
	assert(memchr("hello", 'x', 5) == 0);
	
	// Test memchr with size limits
	assert(memchr("hello", 'o', 4) == 0);      // 'o' is at position 4, but size is 4 (0-3)
	assert(memcmp(memchr("hello", 'l', 3), "llo", 3) == 0);  // finds 'l' at position 2 within size 3
	assert(memchr("hello", 'e', 1) == 0);      // 'e' is at position 1, but size is 1 (0 only)
	
	// Test memchr with zero size
	assert(memchr("hello", 'h', 0) == 0);
	assert(memchr("hello", 'x', 0) == 0);
	
	// Test memchr with null character
	assert(*((char*)memchr("hello", '\0', 6)) == '\0');    // finds null terminator
	assert(memchr("hello", '\0', 5) == 0);     // null is at position 5, size is 5 (0-4)
	
	// Test memchr with single byte
	assert(memcmp(memchr("a", 'a', 1), "a", 1) == 0);
	assert(memchr("a", 'b', 1) == 0);
	
	// Test memchr with repeated characters
	assert(memcmp(memchr("aaaaaa", 'a', 6), "aaaaaa", 6) == 0);  // finds first occurrence
	assert(memcmp(memchr("abcabc", 'a', 6), "abcabc", 6) == 0);  // finds first 'a'
	assert(memcmp(memchr("abcabc", 'b', 6), "bcabc", 5) == 0);   // finds first 'b'
	assert(memcmp(memchr("abcabc", 'c', 6), "cabc", 4) == 0);    // finds first 'c'
	
	// Test memchr with special characters and binary data
	assert(memcmp(memchr("hello world", ' ', 11), " world", 6) == 0);
	assert(memcmp(memchr("a.b,c;d", '.', 7), ".b,c;d", 6) == 0);
	assert(memcmp(memchr("a.b,c;d", ',', 7), ",c;d", 4) == 0);
	assert(memcmp(memchr("a.b,c;d", ';', 7), ";d", 2) == 0);
	
	// Test memchr with binary data (including zeros)
	testmem[0] = 'a';
	testmem[1] = '\0';
	testmem[2] = 'b';
	testmem[3] = '\0';
	testmem[4] = 'c';
	
	assert(memchr(testmem, 'a', 5) == testmem);
	assert(memchr(testmem, '\0', 5) == testmem + 1);  // finds first null
	assert(memchr(testmem, 'b', 5) == testmem + 2);
	assert(memchr(testmem, 'c', 5) == testmem + 4);
	assert(memchr(testmem, 'd', 5) == 0);
	
	// Test memchr with different byte values
	for(int i = 0; i < 256; i++)
	{
		testmem[i] = (unsigned char)i;
	}
	
	// Test finding various byte values
	assert(memchr(testmem, 0, 256) == testmem);        // byte 0 at position 0
	assert(memchr(testmem, 1, 256) == testmem + 1);    // byte 1 at position 1
	assert(memchr(testmem, 127, 256) == testmem + 127); // byte 127 at position 127
	assert(memchr(testmem, 255, 256) == testmem + 255); // byte 255 at position 255
	
	// Test memchr with size smaller than target position
	assert(memchr(testmem, 100, 50) == 0);   // byte 100 is at position 100, but size is 50
	assert(memchr(testmem, 100, 101) == testmem + 100); // finds it with adequate size
	
	// Test memchr with longer memory region
	for(int i = 0; i < 1000; i++)
	{
		testmem[i] = 'a' + (i & 7);
	}
	
	// Test memchr with pattern
	assert(memchr(testmem, 'a', 1000) == testmem);      // first 'a' at start
	assert(memchr(testmem, 'b', 1000) == testmem + 1);  // first 'b' at position 1
	assert(memchr(testmem, 'h', 1000) == testmem + 7);  // first 'h' at position 7
	assert(memchr(testmem, 'z', 1000) == 0);            // 'z' not in pattern
	
	// Test memchr starting from middle of buffer
	assert(memchr(testmem + 100, 'a', 900) == testmem + 104);  // next 'a' after position 100
	assert(memchr(testmem + 100, 'c', 900) == testmem + 106);  // next 'c' after position 100
	
	// Test memchr with exact size to find character
	assert(memchr(testmem, 'h', 8) == testmem + 7);   // 'h' at position 7, size exactly 8
	assert(memchr(testmem, 'h', 7) == 0);             // 'h' at position 7, size only 7 (0-6)
	
	// Test memchr character not found in range
	for(int i = 0; i < 100; i++)
	{
		testmem[i] = 'x';  // fill with 'x'
	}
	assert(memchr(testmem, 'y', 100) == 0);  // 'y' not found
	assert(memchr(testmem, 'x', 100) == testmem);  // 'x' found at start

	return 0;
}
