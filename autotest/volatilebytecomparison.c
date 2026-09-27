/*
 * Regression test for volatile byte comparison folding after passing a struct by value.
 *
 * An int16_t value wrapped inside a struct is passed by value, truncated to uint8_t,
 * stored into a volatile byte location, and then read back and compared with the same
 * truncated struct field value.
 *
 * When global constant propagation ran on intermediate code, relational operators
 * copied forwarded integer constants without limiting their values to the operand
 * type width. Consequently, a 16-bit constant (e.g. 300) was propagated into an 8-bit
 * comparison operand, causing the compiler to incorrectly fold the volatile comparison
 * to constant true and eliminate the runtime check.
 */

#include <stdint.h>

struct SpriteColumnComparisonPosition
{
	int16_t column;
};

static volatile uint8_t hardwareRegister;

static void positionSprite(const struct SpriteColumnComparisonPosition position)
{
	hardwareRegister = (uint8_t)position.column;
}

static int testVolatileByteComparison(void)
{
	const struct SpriteColumnComparisonPosition position = {
		300
	};

	positionSprite(position);

	if (hardwareRegister != (uint8_t)position.column)
		return 1;

	return 0;
}

int main(void)
{
	return testVolatileByteComparison();
}
