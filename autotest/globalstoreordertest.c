/*
 * Global stores must remain ordered around calls which read or modify an
 * address-taken array, including accesses with a known global memory base.
 */

#include <string.h>

static unsigned char storeOrder_values[8];
static unsigned char storeOrder_source[8] = { 3, 4, 5, 6, 7, 8, 9, 10 };

static __noinline void resetValues(void)
{
	memset(storeOrder_values, 0, sizeof(storeOrder_values));
}

static __noinline unsigned char readValue(unsigned char index)
{
	return storeOrder_values[index];
}

static __noinline void copyValues(void)
{
	memcpy(storeOrder_values, storeOrder_source, sizeof(storeOrder_values));
}

static __noinline unsigned char readThroughCall(unsigned char index)
{
	return readValue(index);
}

/**
 * @brief Checks observable store order across direct and transitive calls.
 * @return Zero when all reads observe the correctly sequenced stores.
 */
int main(void)
{
	resetValues();
	storeOrder_values[0] = 1;
	const unsigned char afterReset = readValue(0);

	storeOrder_values[1] = 1;
	const unsigned char beforeReplacement = readValue(1);
	storeOrder_values[1] = 0;
	const unsigned char afterReplacement = readValue(1);

	copyValues();
	storeOrder_values[2] = 1;
	const unsigned char afterCopy = readValue(2);
	const unsigned char untouchedCopy = readValue(3);

	storeOrder_values[4] = 1;
	const unsigned char beforeTransitiveReplacement = readThroughCall(4);
	storeOrder_values[4] = 0;
	const unsigned char afterTransitiveReplacement = readThroughCall(4);

	return afterReset == 1
		&& beforeReplacement == 1 && afterReplacement == 0
		&& afterCopy == 1 && untouchedCopy == 6
		&& beforeTransitiveReplacement == 1 && afterTransitiveReplacement == 0
		? 0 : 1;
}
