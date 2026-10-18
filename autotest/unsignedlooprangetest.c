#include <assert.h>
#include <stdint.h>

// Treat invalid range diagnostics as errors so false positives fail the test.
#pragma warning(error: 2017)

enum
{
	unsignedLoopRange_cellCount = 1000u,
	unsignedLoopRange_pattern = 0x5Au
};

static uint8_t unsignedLoopRange_cells[unsignedLoopRange_cellCount];

// Check both sides of the negative-counter optimisation's iteration limits.
// Volatile accesses keep every iteration observable to the emulator.
static volatile uint16_t unsignedLoopRange_iterations;

#define CHECK_LOOP_COUNT(limit) \
	do { \
		unsignedLoopRange_iterations = 0; \
		for (uint16_t cellIndex = 0u; cellIndex < (limit); ++cellIndex) \
			++unsignedLoopRange_iterations; \
		assert(unsignedLoopRange_iterations == (limit)); \
	} while (0)

/**
 * @brief Checks a bounded unsigned fill and every resulting byte.
 *
 * @return Zero when all thousand cells contain the expected pattern.
 */
int main(void)
{
	for (uint16_t cellIndex = 0u; cellIndex < unsignedLoopRange_cellCount; ++cellIndex)
		unsignedLoopRange_cells[cellIndex] = unsignedLoopRange_pattern;

	for (uint16_t cellIndex = 0u; cellIndex < unsignedLoopRange_cellCount; ++cellIndex)
		assert(unsignedLoopRange_cells[cellIndex] == unsignedLoopRange_pattern);

	CHECK_LOOP_COUNT(255u);
	CHECK_LOOP_COUNT(256u);
	CHECK_LOOP_COUNT(32767u);
	CHECK_LOOP_COUNT(32768u);
	return 0;
}
