#include <assert.h>

// Each selector isolates a C99 enum constraint violation on a 16-bit int target.
enum
{
#if defined(ENUM_RANGE_EXPLICIT)
	enumRange_value = 40000
#elif defined(ENUM_RANGE_UPPER)
	enumRange_value = 32768
#elif defined(ENUM_RANGE_LOWER)
	enumRange_value = -32769L
#elif defined(ENUM_RANGE_IMPLICIT)
	enumRange_previous = 32767,
	enumRange_value
#elif defined(ENUM_RANGE_WIDE)
	enumRange_value = 4294967296UL
#else
	enumRange_minimum = -32768,
	enumRange_value = 32767
#endif
};

/**
 * @brief Verifies that the inclusive int boundaries remain valid enumerators.
 * @return Zero when the valid boundary checks pass.
 */
int main(void)
{
#if !defined(ENUM_RANGE_EXPLICIT) && !defined(ENUM_RANGE_UPPER) && !defined(ENUM_RANGE_LOWER) && !defined(ENUM_RANGE_IMPLICIT) && !defined(ENUM_RANGE_WIDE)
	assert(enumRange_minimum / 2 == -16384);
	assert(enumRange_value == 32767);
	assert(sizeof(enumRange_minimum) == sizeof(int));
	assert(sizeof(enumRange_value) == sizeof(int));
#endif
	return 0;
}
