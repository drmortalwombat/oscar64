#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

// Both enums contain only values representable as int, including on a 16-bit target.
typedef enum
{
	enumPromotion_smallScale = 64
} EnumPromotionSmallStorage;

typedef enum EnumPromotionWordTag
{
	enumPromotion_wordScale = 64,
	enumPromotion_unrelatedWord = 320
} EnumPromotionWordStorage;

typedef enum
{
	enumPromotion_byteMaximum = 255
} EnumPromotionByteStorage;

typedef enum EnumPromotionSignedTag
{
	enumPromotion_minimum = -32768,
	enumPromotion_negativeWord = -129,
	enumPromotion_maximum = 32767
} EnumPromotionSignedStorage;

enum
{
	enumPromotion_derivedProduct = -32 * enumPromotion_wordScale / 76,
	enumPromotion_derivedDivision = -416 / enumPromotion_wordScale
};

typedef enum
{
	enumPromotion_negativeByte = -26
} EnumPromotionSignedByteStorage;

/** @brief Supplies volatile inputs for calculations that must execute at run time. */
struct EnumPromotionInputs
{
	int offset;
	int product;
	int length;
	EnumPromotionSmallStorage smallScale;
	EnumPromotionWordStorage wordScale;
	EnumPromotionSignedStorage negativeWord;
	EnumPromotionSignedByteStorage negativeByte;
	enum EnumPromotionWordTag taggedWordScale;
	enum EnumPromotionSignedTag taggedNegativeWord;
};

static volatile struct EnumPromotionInputs enumPromotion_inputs =
{
	-32, -416, 76, enumPromotion_smallScale, enumPromotion_wordScale,
	enumPromotion_negativeWord, enumPromotion_negativeByte,
	enumPromotion_wordScale, enumPromotion_negativeWord
};

/** @brief Checks promoted variadic arguments and the following argument's alignment. */
static void enumPromotion_checkArguments(int marker, ...)
{
	va_list arguments;
	va_start(arguments, marker);
	assert(va_arg(arguments, int) == 64);
	assert(va_arg(arguments, int) == -129);
	assert(va_arg(arguments, int) == -26);
	assert(va_arg(arguments, int) == marker);
	va_end(arguments);
}

/** @brief Groups the affected enum calculations and their signed and positive controls. */
struct EnumPromotionResults
{
	int smallEnumProduct;
	int wordEnumProduct;
	int wordEnumDivision;
	int signedProduct;
	int signedDivision;
	int positiveProduct;
	int enumIsSigned;
};

/**
 * @brief Demonstrates that an unrelated enumerator must not change signed arithmetic.
 *
 * @return EXIT_SUCCESS when C99 enum-constant typing is preserved; EXIT_FAILURE otherwise.
 */
int main(void)
{
	char formattedArguments[32];

	// Volatile inputs also exercise arithmetic that cannot be folded by the parser.
	assert(enumPromotion_inputs.offset * enumPromotion_wordScale / enumPromotion_inputs.length == -26);
	assert(enumPromotion_inputs.product / enumPromotion_wordScale == -6);
	assert(enumPromotion_inputs.product % enumPromotion_wordScale == -32);
	assert(enumPromotion_inputs.offset < enumPromotion_wordScale);
	assert(sizeof(enumPromotion_smallScale) == sizeof(int));
	assert(sizeof(enumPromotion_byteMaximum) == sizeof(int));
	assert(sizeof(enumPromotion_wordScale) == sizeof(int));
	assert(enumPromotion_minimum == -32768);
	assert(enumPromotion_negativeWord == -129);
	assert(enumPromotion_maximum == 32767);
	assert(enumPromotion_derivedProduct == -26);
	assert(enumPromotion_derivedDivision == -6);
	assert(-enumPromotion_wordScale < 0);
	assert(-enumPromotion_wordScale / 2 == -32);
	assert((-enumPromotion_wordScale >> 1) == -32);
	assert(enumPromotion_negativeWord < 0);
	assert(enumPromotion_negativeWord / 2 == -64);
	assert((enumPromotion_negativeWord >> 1) == -65);
	assert(enumPromotion_inputs.negativeWord < 0);
	assert(enumPromotion_inputs.negativeWord / 2 == -64);
	assert((enumPromotion_inputs.negativeWord >> 1) == -65);
	assert(enumPromotion_inputs.negativeByte < 0);
	assert(enumPromotion_inputs.negativeByte / 4 == -6);
	assert((enumPromotion_inputs.negativeByte >> 1) == -13);
	assert(enumPromotion_inputs.taggedNegativeWord < 0);
	assert(enumPromotion_inputs.taggedNegativeWord / 2 == -64);
	assert((enumPromotion_inputs.taggedNegativeWord >> 1) == -65);
	assert(sizeof(enumPromotion_negativeByte) == sizeof(int));
	assert(sizeof(enumPromotion_negativeWord) == sizeof(int));
	assert(sizeof(enumPromotion_minimum) == sizeof(int));
	assert(sizeof(enumPromotion_maximum) == sizeof(int));
	assert(sizeof(enumPromotion_wordScale + 1u) == sizeof(unsigned int));
	assert(enumPromotion_wordScale + 1u == 65u);
	assert((enumPromotion_negativeWord + 1u) / 2u == (unsigned int)-128 / 2u);
	assert((enumPromotion_inputs.negativeWord + 1u) / 2u == (unsigned int)-128 / 2u);

	enumPromotion_checkArguments(1234, enumPromotion_wordScale,
		enumPromotion_negativeWord, enumPromotion_negativeByte, 1234);
	enumPromotion_checkArguments(1234, enumPromotion_inputs.smallScale,
		enumPromotion_inputs.negativeWord, enumPromotion_inputs.negativeByte, 1234);
	printf("enumerator argument: %d\n", enumPromotion_wordScale);
	sprintf(formattedArguments, "%d %d", enumPromotion_wordScale, enumPromotion_negativeWord);
	assert(strcmp(formattedArguments, "64 -129") == 0);
	sprintf(formattedArguments, "%d %d", enumPromotion_inputs.smallScale, enumPromotion_inputs.negativeByte);
	assert(strcmp(formattedArguments, "64 -26") == 0);

#ifdef __OSCAR64C__
	assert(enumPromotion_inputs.offset * enumPromotion_inputs.smallScale / enumPromotion_inputs.length == -26);
	assert(enumPromotion_inputs.offset < enumPromotion_inputs.smallScale);
	assert(sizeof(EnumPromotionByteStorage) == 1);
	assert(sizeof(EnumPromotionSignedStorage) == 2);
	assert(sizeof(enumPromotion_inputs.smallScale) == 1);
	assert(sizeof(enumPromotion_inputs.wordScale) == 2);
	assert(sizeof(enumPromotion_inputs.negativeByte) == 1);
	assert(sizeof(enumPromotion_inputs.negativeWord) == 2);
	assert(sizeof(enumPromotion_inputs.taggedWordScale) == 2);
	assert(sizeof(enumPromotion_inputs.taggedNegativeWord) == 2);
	// The word enum's unsigned int storage still governs enum variable arithmetic.
	assert(enumPromotion_inputs.offset * enumPromotion_inputs.wordScale / enumPromotion_inputs.length == 835);
	assert(!(enumPromotion_inputs.offset < enumPromotion_inputs.wordScale));
	assert(!(-enumPromotion_inputs.wordScale < 0));
	assert(-enumPromotion_inputs.wordScale / 2 == 32736u);
	assert((-enumPromotion_inputs.wordScale >> 1) == 32736u);
	assert(enumPromotion_inputs.offset * enumPromotion_inputs.taggedWordScale / enumPromotion_inputs.length == 835);
	assert(!(enumPromotion_inputs.offset < enumPromotion_inputs.taggedWordScale));
#endif

	//{ Given identical scales, one grouped with an unrelated word-sized enumerator
	const int negativeOffset = -32;
	const int positiveOffset = 32;
	const int length = 76;
	const int distance = 16;
	const int upwardDirection = -26;
	const int negativeOne = -1;
	const int expectedProduct = -26;
	const int expectedDivision = -6;
	const int expectedPositiveProduct = 26;
	const int expectedSignedComparison = 1;
	//}
	//{ When signed values are multiplied or divided by the enum constants
	const struct EnumPromotionResults results =
	{
		.smallEnumProduct = negativeOffset * enumPromotion_smallScale / length,
		.wordEnumProduct = negativeOffset * enumPromotion_wordScale / length,
		.wordEnumDivision = distance * upwardDirection / enumPromotion_wordScale,
		.signedProduct = negativeOffset * (int)enumPromotion_wordScale / length,
		.signedDivision = distance * upwardDirection / (int)enumPromotion_wordScale,
		.positiveProduct = positiveOffset * enumPromotion_wordScale / length,
		.enumIsSigned = negativeOne < enumPromotion_wordScale
	};
	//}
	//{ Then both enums preserve negative results and agree with explicit signed casts
	printf("small enum product: expected %d, actual %d\n", expectedProduct,
			results.smallEnumProduct);
	printf("word enum product: expected %d, actual %d\n", expectedProduct,
			results.wordEnumProduct);
	printf("word enum division: expected %d, actual %d\n", expectedDivision,
			results.wordEnumDivision);
	printf("signed product control: expected %d, actual %d\n", expectedProduct,
			results.signedProduct);
	printf("signed division control: expected %d, actual %d\n", expectedDivision,
			results.signedDivision);
	printf("positive product control: expected %d, actual %d\n", expectedPositiveProduct,
			results.positiveProduct);
	printf("enum signed comparison: expected %d, actual %d\n", expectedSignedComparison,
			results.enumIsSigned);
	return results.smallEnumProduct == expectedProduct
			&& results.wordEnumProduct == expectedProduct
			&& results.wordEnumDivision == expectedDivision
			&& results.signedProduct == expectedProduct
			&& results.signedDivision == expectedDivision
			&& results.positiveProduct == expectedPositiveProduct
			&& results.enumIsSigned == expectedSignedComparison
			? EXIT_SUCCESS : EXIT_FAILURE;
	//}
}
