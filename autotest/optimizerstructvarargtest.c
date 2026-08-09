/*
 * Regression test for optimizer warning 2007 with fields of a by-value struct
 * forwarded to a variadic call.
 */
#pragma warning(error: 2007)

#include <assert.h>
#include <stdint.h>

struct ViewState
{
	uint16_t topLine;
	uint16_t totalLines;
	uint8_t visibleRows;
};

static volatile uint8_t sink;

static void displayAt(const uint8_t row, ...)
{
	sink = row;
}

static void renderView(const struct ViewState state)
{
	displayAt(
		23u,
		state.topLine + 1u,
		state.topLine + state.visibleRows > state.totalLines
			? state.totalLines
			: state.topLine + state.visibleRows,
		state.totalLines
	);
}

int main(void)
{
	const struct ViewState state = { 0u, 30u, 23u };
	renderView(state);
	assert(sink == 23u);
	return 0;
}
