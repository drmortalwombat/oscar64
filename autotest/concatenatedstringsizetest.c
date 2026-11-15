#include <assert.h>
#include <string.h>

int main(void)
{
	/* Concatenation retains all fragments and a single final terminator. */
	assert(sizeof("First line\n"
			"Second line\nThird line\n"
			"Final line")
			== sizeof("First line\n")
			+ sizeof("Second line\nThird line\n")
			+ sizeof("Final line") - 2u);

	/* Empty fragments, multiple fragments and embedded nulls retain their extent. */
	assert(sizeof("abc") == 4);
	assert(sizeof("a" "bc") == 4);
	assert(sizeof("a" "b" "c") == 4);
	assert(sizeof("" "abc") == 4);
	assert(sizeof("abc" "") == 4);
	assert(sizeof("" "") == 1);
	assert(sizeof("a\0" "bc") == 5);
	assert(sizeof(*(&"a" "bc")) == 4);

	/* Check both the expression's data and the separate array-initialiser path. */
	const char * text = "a" "b" "c";
	char buffer[sizeof("a" "bc")];
	char initialised[] = "a" "bc";
	assert(sizeof(buffer) == 4);
	assert(sizeof(initialised) == 4);
	assert(strcmp(text, initialised) == 0);
	memcpy(buffer, text, sizeof(buffer));
	assert(buffer[0] == 'a');
	assert(buffer[1] == 'b');
	assert(buffer[2] == 'c');
	assert(buffer[3] == 0);
	return 0;
}
