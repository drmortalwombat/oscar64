#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Regression: fixed-count loop optimisation must not carry Y or move pointer
 * updates across calls. Keep the default optimisation level in the test matrix;
 * testing only -O2 hides the original native-code failure.
 */
enum
{
	localStructureStride_itemCount = 4u
};

/**
 * @brief Keeps four independent bytes together to reproduce native local-array indexing.
 */
struct LocalStructureStrideItem
{
	uint8_t first;
	uint8_t second;
	uint8_t kind;
	bool flag;
};

static void localStructureStride_checkItem(const struct LocalStructureStrideItem item)
{
	assert(item.kind < localStructureStride_itemCount);
	assert(item.first == item.second);
	assert( ! item.flag);
}

static void localStructureStride_checkItemPtr(const struct LocalStructureStrideItem * item)
{
	assert(item->kind < localStructureStride_itemCount);
	assert(item->first == item->second);
	assert( ! item->flag);
}

/**
 * @brief Checks that native structure-array iteration advances by a complete item.
 *
 * @return Zero when every item passed by value retains its initialised bytes.
 */
int main(void)
{
	const struct LocalStructureStrideItem items[localStructureStride_itemCount] = {
		{1u, 1u, 3u, false},
		{4u, 4u, 1u, false},
		{8u, 8u, 0u, false},
		{15u, 15u, 2u, false}
	};

	for (uint8_t itemIndex = 0u; itemIndex < localStructureStride_itemCount; ++itemIndex)
		localStructureStride_checkItem(items[itemIndex]);
	for (uint8_t itemIndex = 0u; itemIndex < localStructureStride_itemCount; ++itemIndex)
		localStructureStride_checkItemPtr(items + itemIndex);
	return 0;
}
