/*
 * Regression test for signed-byte promotion after a struct return by value.
 *
 * A four-byte struct is returned in the byte-code accumulator.  Extracting its
 * signed first byte and promoting it to int used to leave the accumulator with
 * the zero-extended value 209 instead of the sign-extended value -47.  The
 * byte-code optimizer did not model BC_CONV_I8_I16 as reading and changing its
 * selected register, and consequently removed a required accumulator reload.
 */

#include <stdint.h>

struct SignedByteResult
{
	int8_t value;
	uint8_t secondValue;
	uint8_t thirdValue;
	uint8_t fourthValue;
};

static volatile int8_t sourceValue = -47;
static uint8_t failed;

static struct SignedByteResult returnSignedByteInStruct(void)
{
	const struct SignedByteResult result = {
		.value = sourceValue,
		.secondValue = 0,
		.thirdValue = 0,
		.fourthValue = 0
	};

	return result;
}

static int8_t clampSignedByte(int16_t value, int16_t minimum, int16_t maximum)
{
	if (value < minimum)
		return (int8_t)minimum;
	if (value > maximum)
		return (int8_t)maximum;
	return (int8_t)value;
}

static void testSignedBytePromotion(void)
{
	const struct SignedByteResult result = returnSignedByteInStruct();
	const int16_t promoted = result.value;
	const long clamped = clampSignedByte(promoted, -50, 55);
	const long promotedLong = promoted;

	if (clamped != promotedLong)
		failed++;
}

int main(void)
{
	testSignedBytePromotion();
	return failed;
}
