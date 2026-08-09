#include <stdio.h>

/*
 * Regression test for native initialization of a local pointer array from
 * consecutive members of a struct reached through a function parameter.
 */

struct Attributes
{
	unsigned char atrib1;
	unsigned char atrib2;
	unsigned char atrib3;
	unsigned char atrib4;
	unsigned char atrib5;
};

static int readMembers(struct Attributes* const attributes)
{
	unsigned char* const members[5] = {
		&attributes->atrib1,
		&attributes->atrib2,
		&attributes->atrib3,
		&attributes->atrib4,
		&attributes->atrib5
	};
	unsigned char memberIndex;
	unsigned char expected = 1;
	int result = 0;

	for (memberIndex = 0; memberIndex < 5; ++memberIndex)
	{
		const unsigned char actual = *members[memberIndex];
		printf("%u ", actual);

		if (actual != expected)
			result = 1;
		++expected;
	}
	printf("\n");
	return result;
}

int main(void)
{
	struct Attributes attributes = {1, 2, 3, 4, 5};
	return readMembers(&attributes);
}
