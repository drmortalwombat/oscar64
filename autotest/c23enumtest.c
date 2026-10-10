#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef ENUM_RANGE_WARNING_ERROR
#pragma warning(error: 2017)
#endif

#if defined(C23_ENUM_BYTE_LOWER)
enum C23EnumInvalid : signed char { c23Enum_invalid = -129 };
#elif defined(C23_ENUM_BYTE_UPPER)
enum C23EnumInvalid : signed char { c23Enum_invalid = 128 };
#elif defined(C23_ENUM_UNSIGNED_LOWER)
enum C23EnumInvalid : unsigned char { c23Enum_invalid = -1 };
#elif defined(C23_ENUM_UNSIGNED_UPPER)
enum C23EnumInvalid : unsigned char { c23Enum_invalid = 256 };
#elif defined(C23_ENUM_WORD_UPPER)
enum C23EnumInvalid : int { c23Enum_invalid = 40000 };
#elif defined(C23_ENUM_WORD_LOWER)
enum C23EnumInvalid : int { c23Enum_invalid = -32769L };
#elif defined(C23_ENUM_UNSIGNED_WORD_UPPER)
enum C23EnumInvalid : unsigned int { c23Enum_invalid = 65536L };
#elif defined(C23_ENUM_LONG_UPPER)
enum C23EnumInvalid : long { c23Enum_invalid = 2147483648UL };
#elif defined(C23_ENUM_UNSIGNED_LONG_UPPER)
enum C23EnumInvalid : unsigned long { c23Enum_invalid = 4294967296UL };
#elif defined(C23_ENUM_IMPLICIT_BYTE)
enum C23EnumInvalid : unsigned char { c23Enum_last = 255, c23Enum_invalid };
#elif defined(C23_ENUM_IMPLICIT_WORD)
enum C23EnumInvalid : unsigned int { c23Enum_last = 65535u, c23Enum_invalid };
#elif defined(C23_ENUM_IMPLICIT_LONG)
enum C23EnumInvalid : unsigned long { c23Enum_last = 4294967295UL, c23Enum_invalid };
#elif defined(C23_ENUM_BASE_FLOAT)
enum C23EnumInvalid : float { c23Enum_invalid = 0 };
#elif defined(C23_ENUM_BASE_ENUM)
enum C23EnumBase { c23Enum_base = 0 };
enum C23EnumInvalid : enum C23EnumBase { c23Enum_invalid = 0 };
#elif defined(C23_ENUM_BASE_POINTER)
enum C23EnumInvalid : int * { c23Enum_invalid = 0 };
#elif defined(C23_ENUM_MISMATCH_SIZE)
enum C23EnumInvalid : unsigned char;
enum C23EnumInvalid : unsigned int { c23Enum_invalid = 0 };
#elif defined(C23_ENUM_MISMATCH_SIGN)
enum C23EnumInvalid : unsigned int;
enum C23EnumInvalid : int { c23Enum_invalid = 0 };
#elif defined(C23_ENUM_PLAIN_TO_FIXED)
enum C23EnumInvalid { c23Enum_value = 0 };
enum C23EnumInvalid : unsigned int;
#elif defined(C23_ENUM_NON_STANDALONE)
enum C23EnumInvalid : unsigned int c23Enum_invalid;
#else
#define C23_ENUM_VALID

typedef const unsigned char C23EnumByteType;

enum C23EnumByte : C23EnumByteType
{
	c23Enum_byteScale = 64,
	c23Enum_byteMaximum = 255,
	c23Enum_byteWrapped = (unsigned char)(c23Enum_byteMaximum + 1)
};

enum C23EnumSignedByte : signed char
{
	c23Enum_byteMinimum = -128,
	c23Enum_byteNegative = -26,
	c23Enum_signedByteMaximum = 127
};

enum C23EnumWord : unsigned int
{
	c23Enum_wordScale = 64,
	c23Enum_large = 40000,
	c23Enum_wordMaximum = 65535u,
	c23Enum_wordWrapped = c23Enum_wordMaximum + 1u
};

enum C23EnumSignedWord : int
{
	c23Enum_wordMinimum = -32768,
	c23Enum_wordNegative = -129,
	c23Enum_signedWordMaximum = 32767
};

enum C23EnumLong : long
{
	c23Enum_longMinimum = -2147483647L - 1,
	c23Enum_longMaximum = 2147483647L
};

enum C23EnumUnsignedLong : unsigned long
{
	c23Enum_longUnsignedMaximum = 4294967295UL
};

enum C23EnumForward : unsigned int;
static volatile enum C23EnumForward c23Enum_forward = 50000u;
enum C23EnumForward : unsigned int { c23Enum_forwardNamed = 50000u };
enum C23EnumForward : unsigned int;

enum C23EnumAnonymous : const unsigned char { c23Enum_qualified = 7 };
enum : unsigned char { c23Enum_anonymous = 9 };

/** @brief Supplies volatile enum and integer inputs to exercise generated arithmetic. */
struct C23EnumInputs
{
	enum C23EnumByte byteScale;
	enum C23EnumSignedByte negativeByte;
	enum C23EnumWord wordScale;
	enum C23EnumSignedWord negativeWord;
	int negativeOffset;
	int length;
};

static volatile struct C23EnumInputs c23Enum_inputs =
{
	c23Enum_byteScale, c23Enum_byteNegative, c23Enum_wordScale,
	c23Enum_wordNegative, -32, 76
};

/** @brief Verifies the default promotions and alignment of variadic enum arguments. */
static void c23Enum_checkArguments(int marker, ...)
{
	va_list arguments;
	va_start(arguments, marker);
	assert(va_arg(arguments, int) == 64);
	assert(va_arg(arguments, int) == -26);
	assert(va_arg(arguments, unsigned int) == 40000u);
	assert(va_arg(arguments, unsigned long) == 4294967295UL);
	assert(va_arg(arguments, int) == marker);
	va_end(arguments);
}
#endif

/**
 * @brief Verifies explicitly typed C enums; selectors instead trigger required diagnostics.
 * @return Zero when the valid enum checks pass.
 */
int main(void)
{
#ifdef C23_ENUM_VALID
	char formatted[32];
	assert(sizeof(c23Enum_byteScale) == sizeof(unsigned char));
	assert(sizeof(c23Enum_byteNegative) == sizeof(signed char));
	assert(sizeof(c23Enum_large) == sizeof(unsigned int));
	assert(sizeof(c23Enum_longMaximum) == sizeof(long));
	assert(sizeof(enum C23EnumByte) == sizeof(unsigned char));
	assert(sizeof(enum C23EnumForward) == sizeof(unsigned int));
	assert(c23Enum_large == 40000u);
	assert(c23Enum_byteWrapped == 0);
	assert(c23Enum_wordWrapped == 0u);
	assert(c23Enum_longMinimum / 2 == -1073741824L);
	assert(c23Enum_longUnsignedMaximum / 2UL == 2147483647UL);
	assert(c23Enum_qualified == 7);
	assert(c23Enum_anonymous == 9);
	assert(c23Enum_forward == c23Enum_forwardNamed);
	assert(c23Enum_forward > 40000u);
	assert((enum C23EnumByte)200 == 200);
	assert((enum C23EnumSignedByte)-100 == -100);
	assert(c23Enum_inputs.negativeByte < 0);
	assert(c23Enum_inputs.negativeByte / 4 == -6);
	assert((c23Enum_inputs.negativeByte >> 1) == -13);
	assert(c23Enum_inputs.negativeWord / 2 == -64);
	assert((c23Enum_inputs.negativeWord >> 1) == -65);
	assert(c23Enum_inputs.negativeOffset * c23Enum_inputs.byteScale / c23Enum_inputs.length == -26);
	assert(c23Enum_inputs.negativeOffset * c23Enum_wordScale / c23Enum_inputs.length == 835);
	assert(c23Enum_inputs.negativeOffset * c23Enum_inputs.wordScale / c23Enum_inputs.length == 835);
	assert(-c23Enum_byteScale / 2 == -32);
	assert(-c23Enum_inputs.byteScale / 2 == -32);
	assert(sizeof(-c23Enum_inputs.byteScale) == sizeof(int));
	assert(sizeof(c23Enum_inputs.byteScale >> 1) == sizeof(int));
	assert((-c23Enum_inputs.byteScale >> 1) == -32);
	assert(c23Enum_inputs.negativeOffset < c23Enum_byteScale);
	assert(!(c23Enum_inputs.negativeOffset < c23Enum_wordScale));
	c23Enum_checkArguments(1234, c23Enum_byteScale, c23Enum_byteNegative,
		c23Enum_large, c23Enum_longUnsignedMaximum, 1234);
	sprintf(formatted, "%d %u", c23Enum_inputs.negativeByte, c23Enum_large);
	assert(strcmp(formatted, "-26 40000") == 0);
#endif
	return 0;
}
