/**
 * @file
 * @brief Reproduces bytecode parameter registers overlapping an indirect-call descendant.
 *
 * The service callback stays NULL throughout execution. Its signature matches the indirect
 * test entry, creating a possible cycle for call-graph analysis without runtime recursion.
 * Retain main's initial advance call: the call-discovery order exposes the invalid allocation.
 * Drawing must write zero whilst the saved attribute remains six.
 * Keep the pull helper out of line to exercise parameter preservation.
 */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <c64/vic.h>

enum
{
	attributeRegister_borderColour = 2u,
	attributeRegister_colourMask = 1u,
	attributeRegister_expectedStrength = 6u
};

/** @brief Invokes a background service or a test body. */
typedef void (*AttributeRegisterCallback)(void);
/** @brief Retains a nullable background callback. */
struct AttributeRegisterService
{
	AttributeRegisterCallback callback;
};
static struct AttributeRegisterService attributeRegister_service;
static uint8_t attributeRegister_strength;

static __noinline void attributeRegister_draw(const uint8_t value)
{
	vic.spr_msbx = value;
}

static __noinline void attributeRegister_advance(void)
{
	if ((&attributeRegister_service)->callback != NULL)
		(&attributeRegister_service)->callback();
	attributeRegister_draw(vic.color_border & attributeRegister_colourMask);
}

// The live attribute must survive the descendant's draw argument.
static __noinline void attributeRegister_pull(const uint8_t strength)
{
	attributeRegister_advance();
	attributeRegister_strength = strength;
}

static void attributeRegister_test(void)
{
	attributeRegister_pull(attributeRegister_expectedStrength);
}

/** @brief Retains an indirect test entry point with the service callback's signature. */
struct AttributeRegisterTest
{
	AttributeRegisterCallback test;
};
static struct AttributeRegisterTest attributeRegister_testCase = { attributeRegister_test };

/**
 * @brief Exercises both call paths without registering the optional service callback.
 * @return Zero if the attribute survives the descendant's draw call.
 */
int main(void)
{
	vic.color_border = attributeRegister_borderColour;
	attributeRegister_advance();
	if ((&attributeRegister_testCase)->test != NULL)
		(&attributeRegister_testCase)->test();
	assert(vic.spr_msbx == 0);
	assert(attributeRegister_strength == attributeRegister_expectedStrength);
	return 0;
}
