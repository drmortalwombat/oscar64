#include <assert.h>

// C++ enumerators must retain their enum type for overload resolution.
enum EnumConstantTypeWord
{
	enumConstantType_scale = 64,
	enumConstantType_word = 320,
	enumConstantType_large = 40000
};

enum class EnumConstantTypeScoped : unsigned int
{
	scale = 64,
	maximum = 65535
};

enum EnumConstantTypeByte : unsigned char
{
	enumConstantType_byteMaximum = 255
};

enum EnumConstantTypeSigned : int
{
	enumConstantType_negative = -26
};

/** @brief Identifies the enum overload chosen for a word-sized enumerator. */
static int enumConstantType_identify(EnumConstantTypeWord value)
{
	return value == enumConstantType_scale;
}

/** @brief Identifies the integer overload, which an enum argument must not select. */
static int enumConstantType_identify(int value)
{
	return 0;
}

/**
 * @brief Verifies enum overloads, scoped values and explicit underlying types.
 * @return Zero when all enum compatibility checks pass.
 */
int main(void)
{
	enum EnumConstantTypeWord wordScale = enumConstantType_scale;
	volatile enum EnumConstantTypeSigned negative = enumConstantType_negative;
	assert(sizeof(wordScale) == sizeof(EnumConstantTypeWord));
	assert(enumConstantType_identify(wordScale) == 1);
	assert(negative < 0);
	assert(negative / 4 == -6);
	assert((negative >> 1) == -13);
	assert(enumConstantType_identify(enumConstantType_scale) == 1);
	assert(enumConstantType_large == 40000u);
	assert(sizeof(enumConstantType_scale) == sizeof(EnumConstantTypeWord));
	assert(sizeof(enumConstantType_byteMaximum) == sizeof(unsigned char));
	assert(sizeof(EnumConstantTypeScoped) == sizeof(unsigned int));
	assert((unsigned int)EnumConstantTypeScoped::maximum == 65535u);
	assert(enumConstantType_negative / 4 == -6);
	return 0;
}
