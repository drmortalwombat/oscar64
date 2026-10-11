#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum Item
{
	ITEM_NONE,
	ITEM_PRESENT
};

enum { ITEM_COUNT = 9 };

struct Values
{
	enum Item values[ITEM_COUNT];
};

struct Container
{
	enum Item values[ITEM_COUNT];
	struct Values nested;
};

static uint8_t count(const enum Item values[], uint8_t size)
{
	uint8_t result = 0;
	for (uint8_t i = 0; i < size; ++i)
		if (values[i] == ITEM_PRESENT)
			++result;
	return result;
}

static uint8_t direct(const struct Container *container)
{
	return count(container->values, ITEM_COUNT);
}

static uint8_t nested(const struct Container *container)
{
	return count(container->nested.values, ITEM_COUNT);
}

static uint8_t conditional(const struct Container *container, bool useNested)
{
	const enum Item *const values = useNested
		? container->nested.values : container->values;
	return count(values, ITEM_COUNT);
}

// Repeated qualifiers and either qualifier position must describe the same type.
static uint8_t forwarded(const enum Item const values[], uint8_t size)
{
	return count(values, size);
}

static uint8_t postfix(enum Item const values[], uint8_t size)
{
	return count(values, size);
}

static enum Item readVolatile(volatile enum Item *value)
{
	volatile enum Item *forwarded = value;
	return *forwarded;
}

static enum Item readConstVolatile(const volatile enum Item *value)
{
	volatile const enum Item *forwarded = value;
	return *forwarded;
}

int main(void)
{
	struct Container container = {
		.values = { ITEM_PRESENT },
		.nested = { .values = { ITEM_PRESENT, ITEM_PRESENT } }
	};

	for (uint8_t expected = 1; ; expected = 0)
	{
		assert(direct(&container) == expected);
		assert(nested(&container) == 2 * expected);
		assert(conditional(&container, false) == expected);
		assert(conditional(&container, true) == 2 * expected);
		assert(forwarded(container.values, ITEM_COUNT) == expected);
		assert(postfix(container.values, ITEM_COUNT) == expected);
		assert(readVolatile(&container.values[0]) == expected);
		assert(readConstVolatile(&container.nested.values[0]) == expected);
		assert(container.values[0] == expected);
		assert(container.nested.values[0] == expected);

		if (expected == 0)
			break;
		// Qualification of a reference must leave the original enum tag mutable.
		container.values[0] = ITEM_NONE;
		container.nested.values[0] = ITEM_NONE;
		container.nested.values[1] = ITEM_NONE;
	}
	return 0;
}
