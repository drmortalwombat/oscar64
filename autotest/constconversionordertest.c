/*
 * Complete the type before the shared declarations, as in the original caller.
 * The header queues the implementation before a later translation unit which
 * redeclares both functions using its own incomplete structure declaration.
 */
struct ConstConversionOrderTimer
{
	unsigned secondsRemaining;
};

#include "constconversionordertest.h"

int main(void)
{
	struct ConstConversionOrderTimer timer = { 0u };

	constConversionOrderDisplay(&timer);
	return 0;
}
