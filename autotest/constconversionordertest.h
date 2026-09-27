#ifndef CONST_CONVERSION_ORDER_TEST_H
#define CONST_CONVERSION_ORDER_TEST_H

struct ConstConversionOrderTimer;

void constConversionOrderInvalidate(void);
void constConversionOrderDisplay(const struct ConstConversionOrderTimer* const timer);

#pragma compile("constconversionordertest_impl.c")
#pragma compile("constconversionordertest_later.c")

#endif
