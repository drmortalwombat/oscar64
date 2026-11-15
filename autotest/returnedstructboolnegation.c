/**
 * @file
 * @brief Reproduces bytecode negation testing the wrong byte of a returned struct's bool.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
static volatile bool sourceExhausted;

/** @brief Packs three counters and an exhaustion flag into a four-byte returned value. */
struct ReturnedStructBoolStatus
{
	uint8_t stress;
	uint8_t energy;
	uint8_t pulls;
	bool isExhausted;
};

static __noinline struct ReturnedStructBoolStatus returnedStructBool_status(const bool exhausted)
{
	const struct ReturnedStructBoolStatus status = { 8u, 15u, 1u, exhausted };
	return status;
}

static __noinline uint16_t returnedStructBool_interval(const bool exhausted)
{
	const struct ReturnedStructBoolStatus status = returnedStructBool_status(exhausted);
	return ! status.isExhausted ? 350u : 0u;
}

/** @brief Checks both exhaustion states. @return Zero when exhaustion prevents elapsed time. */
int main(void)
{
	sourceExhausted = false;
	const bool exhausted = sourceExhausted;
	assert(returnedStructBool_interval(exhausted) == (exhausted ? 0u : 350u));
	assert(returnedStructBool_interval(! exhausted) == (exhausted ? 350u : 0u));
	return 0;
}
