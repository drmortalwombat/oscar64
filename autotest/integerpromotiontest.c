/**
 * @file
 * @brief Verifies promoted integer expression sizes and arithmetic on the 16-bit target.
 * Run: make -C autotest integerpromotiontest OSCAR64_CC=../bin/oscar64
 * The standard Makefile and Windows autotest.bat pipelines include this test.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum IntegerPromotionEnum { integerPromotion_enumValue = 64 };

static volatile char integerPromotion_char = 1;
static volatile signed char integerPromotion_signedChar = -26;
static volatile unsigned char integerPromotion_unsignedChar = 255;
static volatile bool integerPromotion_bool = true;
static volatile enum IntegerPromotionEnum integerPromotion_enum = integerPromotion_enumValue;
static volatile short integerPromotion_short = -26;
static volatile unsigned short integerPromotion_unsignedShort = 40000u;
static volatile int integerPromotion_int = -26;
static volatile unsigned int integerPromotion_unsignedInt = 40000u;
static volatile long integerPromotion_long = -26L;
static volatile unsigned long integerPromotion_unsignedLong = 40000UL;

/** @brief Checks promoted expression sizes without evaluating the operand. */
#define CHECK_PROMOTED_SIZE(value, type) \
	do { \
		assert(sizeof(+(value)) == sizeof(type)); \
		assert(sizeof(-(value)) == sizeof(type)); \
		assert(sizeof(~(value)) == sizeof(type)); \
		assert(sizeof((value) << 1) == sizeof(type)); \
		assert(sizeof((value) >> 1) == sizeof(type)); \
	} while (0)

/** @brief Verifies integer promotions in parsed and folded expressions. */
int main(void)
{
	char k = 1;
	char formatted[16];
	sprintf(formatted, "%d, %d", sizeof(k), sizeof(+k));
	assert(strcmp(formatted, "1, 2") == 0);
	assert(sizeof(++k) == sizeof(char));
	assert(sizeof(k++) == sizeof(char));
	assert(sizeof(*(&k)) == sizeof(char));
	assert(k == 1);
#ifdef __cplusplus
	char& reference = k;
	CHECK_PROMOTED_SIZE(reference, int);
	assert(+reference == 1);
#endif

	assert(sizeof(integerPromotion_char) == 1);
	CHECK_PROMOTED_SIZE(integerPromotion_char, int);
	CHECK_PROMOTED_SIZE(integerPromotion_signedChar, int);
	CHECK_PROMOTED_SIZE(integerPromotion_unsignedChar, int);
	CHECK_PROMOTED_SIZE(integerPromotion_bool, int);
	CHECK_PROMOTED_SIZE(integerPromotion_enum, int);
	CHECK_PROMOTED_SIZE(integerPromotion_short, int);
	CHECK_PROMOTED_SIZE(integerPromotion_unsignedShort, unsigned int);
	CHECK_PROMOTED_SIZE(integerPromotion_int, int);
	CHECK_PROMOTED_SIZE(integerPromotion_unsignedInt, unsigned int);
	CHECK_PROMOTED_SIZE(integerPromotion_long, long);
	CHECK_PROMOTED_SIZE(integerPromotion_unsignedLong, unsigned long);

	CHECK_PROMOTED_SIZE((signed char)-26, int);
	CHECK_PROMOTED_SIZE((unsigned char)255, int);
	CHECK_PROMOTED_SIZE((bool)true, int);
	assert(sizeof(+integerPromotion_enumValue) == sizeof(int));
	assert(sizeof((unsigned char)1 << 1L) == sizeof(int));
	assert(sizeof((unsigned char)1 >> 1UL) == sizeof(int));

	assert(-integerPromotion_unsignedChar == -255);
	assert(~integerPromotion_unsignedChar == -256);
	assert(+integerPromotion_signedChar < 0);
	assert(+integerPromotion_signedChar / 4 == -6);
	assert((integerPromotion_signedChar >> 1) == -13);
	assert((integerPromotion_unsignedChar << 1) == 510);
	assert((integerPromotion_unsignedChar >> 1UL) == 127);
	assert(((unsigned char)255 << 1L) == 510);
	assert(-((unsigned char)255) == -255);
	assert(~((unsigned char)255) == -256);
	assert(-integerPromotion_bool == -1);
	assert(~integerPromotion_bool == -2);
	assert(-integerPromotion_enum == -64);
	assert((integerPromotion_unsignedInt << 1L) == 14464u);
	assert(((unsigned int)40000u << 1L) == 14464u);
	assert((integerPromotion_long >> 1) == -13L);
	assert((integerPromotion_unsignedLong << 1) == 80000UL);
	return 0;
}
