/**
 * @file
 * @brief Reproduces native saved-register overlap across a conditional call argument.
 *
 * Three calls must each return 100 + 3 + 3, giving 318. A miscompiled native
 * build fails that assertion when draw overwrites control's saved-register area.
 * The second assertion checks the independently calculated drawing subtotal, 309.
 *
 * Evaluating the direction in a preceding if statement makes the unfixed compiler pass without
 * changing compiler options. Retain the compound condition, nested heading call and live
 * retained value. The eight running totals keep native register-save pressure in draw;
 * reducing them can mask the overlap. __noinline keeps the affected call boundaries.
 * Observed with Oscar64 1.32.273 and -tm=c64 -tf=prg -n -ea -gp -strict -psci.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <c64/vic.h>

static uint8_t conditionalCall_input;
static uint16_t conditionalCall_total;
static bool conditionalCall_wasAiming;

static __noinline uint8_t conditionalCall_heading(void)
{
	return conditionalCall_input;
}

static __noinline void conditionalCall_draw(const uint8_t value)
{
	uint16_t subtotal = value;
	uint16_t second = value + 2u;
	uint16_t third = value + 3u;
	uint16_t fourth = value + 4u;
	uint16_t fifth = value + 5u;
	uint16_t sixth = value + 6u;
	uint16_t seventh = value + 7u;
	uint16_t eighth = value + 8u;
	for (uint8_t index = 0u; index < 12u; ++index)
	{
		subtotal += (uint16_t)(conditionalCall_input + index) * value;
		second += subtotal;
		third += second;
		fourth += third;
		fifth += fourth;
		sixth += fifth;
		seventh += sixth;
		eighth += seventh;
	}
	conditionalCall_total = subtotal;
	// Keep the running totals observable through Oscar64's volatile hardware interface.
	vic.spr_msbx = (uint8_t)eighth;
}

static __noinline void conditionalCall_shoot(const uint8_t direction, const bool fire,
		const uint16_t milliseconds, const bool show)
{
	if (fire || show)
		conditionalCall_draw(direction + milliseconds);
}

static __noinline uint16_t conditionalCall_control(const bool aiming, const uint16_t health)
{
	const uint16_t retained = health + (vic.color_border & 15u);
	const bool released = conditionalCall_wasAiming && ! aiming;
	conditionalCall_shoot(aiming || released ? conditionalCall_heading() : 1u,
			released, 0u, aiming);
	conditionalCall_wasAiming = aiming;
	return retained + conditionalCall_input;
}

/**
 * @brief Checks the retained caller value and the independently calculated drawing subtotal.
 * @return Zero if both postconditions hold; an assertion terminates a miscompiled run.
 */
int main(void)
{
	vic.color_border = 3u;
	conditionalCall_input = vic.color_border & 15u;
	uint16_t result = 0u;
	for (uint8_t iteration = 0u; iteration < conditionalCall_input; ++iteration)
		result += conditionalCall_control(conditionalCall_input != 0u, 100u);
	assert(result == 318u);
	assert(conditionalCall_total == 309u);
	return 0;
}
