/**
 * @file
 * @brief Checks that native code distinguishes a page-aligned pointer from null.
 *
 * The first slot at $cc00 has the same low byte as null. All five slot pointers
 * must pass the null check, and the loop must count four occupied slots.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

enum
{
	pageAlignedArray_slotCount = 5u,
	pageAlignedArray_paddingSize = 32u
};

static const uint16_t pageAlignedArray_base = 0xcc00u;

/** @brief Gives each slot a distinct low address within the same memory page. */
struct PageAlignedArraySlot
{
	uint8_t padding[pageAlignedArray_paddingSize];
	uint8_t value;
};

static struct PageAlignedArraySlot* const pageAlignedArray_slots =
		(struct PageAlignedArraySlot*)pageAlignedArray_base;

static __noinline uint8_t pageAlignedArray_countOccupied(void)
{
	uint8_t occupied = 0u;

	for (uint8_t slotIndex = 0u; slotIndex < pageAlignedArray_slotCount; ++slotIndex)
	{
		const struct PageAlignedArraySlot* const slot = &pageAlignedArray_slots[slotIndex];
		assert(slot != NULL);
		if (slot->value != 0u)
			++occupied;
	}
	return occupied;
}

int main(void)
{
	for (uint8_t slotIndex = 0u; slotIndex < pageAlignedArray_slotCount; ++slotIndex)
		pageAlignedArray_slots[slotIndex].value = slotIndex;
	assert(pageAlignedArray_countOccupied() == pageAlignedArray_slotCount - 1u);
	return 0;
}
