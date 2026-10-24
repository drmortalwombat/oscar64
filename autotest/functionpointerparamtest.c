#include <assert.h>

typedef int (*BinaryFunction)(int, int);

__noinline int add(int a, int b)
{
	return a + b;
}

__noinline int subtract(int a, int b)
{
	return a - b;
}

int main(void)
{
	// Taking the address first must prevent later constant specialization.
	BinaryFunction sum = add;
	assert(add(3, 4) == 7);
	assert(sum(10, 20) == 30);

	// Taking the address later must also clear previously recorded constants.
	assert(subtract(9, 2) == 7);
	BinaryFunction difference = &subtract;
	assert(difference(50, 8) == 42);

	return 0;
}
