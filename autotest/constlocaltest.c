#include <assert.h>
#include <stdbool.h>

static int counter;

static int read_const_local_twice(void)
{
	const int value = ++counter;
	return value + value;
}

static bool update(unsigned char* value)
{
	const unsigned char before = *value;
	const unsigned char after = before + 2;
	const bool changed = after != before;

	*value = after;
	return changed;
}

int main(void)
{
	assert(read_const_local_twice() == 2);
	assert(counter == 1);

	unsigned char value = 7;
	assert(update(&value));
	assert(value == 9);

	return 0;
}
