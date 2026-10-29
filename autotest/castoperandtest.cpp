/* Keep expression parsing inside a cast operand's parentheses. */
#include <assert.h>
#include <stdint.h>

static unsigned constructorCalls;

struct Number
{
	int value;

	Number(int initial) : value(initial)
	{
		constructorCalls++;
	}

	operator int() const
	{
		return value;
	}
};

int main(void)
{
	/* Parsing these parentheses as a declaration breaks the constructor call. */
	assert((int)(Number(202)) == 202);
	assert(constructorCalls == 1u);
	assert((int)(uint16_t(203)) == 203);
	assert((int)(Number(204).value) == 204);
	assert(constructorCalls == 2u);

	/* A postfix member access outside the parentheses still belongs to the operand. */
	assert((int)(Number(205)).value == 205);
	assert(constructorCalls == 3u);
	int value = 206;
	assert((int)(int&)(value) == 206);
	return 0;
}
