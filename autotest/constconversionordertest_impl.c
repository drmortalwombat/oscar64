#include "constconversionordertest.h"

struct ConstConversionOrderTimer
{
	unsigned secondsRemaining;
};

void constConversionOrderInvalidate(void)
{
}

void constConversionOrderDisplay(const struct ConstConversionOrderTimer* const timer)
{
	(void)timer;
}
