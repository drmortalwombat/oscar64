#include <c64/memmap.h>
#include <c64/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define CONDITIONAL_PANEL_COPY__BITMAP_ADDRESS (0xE000u)
#define CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS (0xC800u)
#define CONDITIONAL_PANEL_COPY__COLOUR_ADDRESS (0xD800u)

enum
{
	conditionalPanelCopy_columns = 11u,
	conditionalPanelCopy_rows = 8u,
	conditionalPanelCopy_firstCell = 960u,
	conditionalPanelCopy_pattern = 0x26u
};

/** @brief Groups the saved bitmap and palette planes for a conditional copy. */
struct ConditionalPanelCopyBackground
{
	byte bitmap[conditionalPanelCopy_columns * conditionalPanelCopy_rows];
	byte screenMatrix[conditionalPanelCopy_columns];
	byte colourRam[conditionalPanelCopy_columns];
};

static struct ConditionalPanelCopyBackground conditionalPanelCopy_background;

static void conditionalPanelCopy_preserveBackground(const bool shouldRestore)
{
	byte* const bitmap = (byte*)CONDITIONAL_PANEL_COPY__BITMAP_ADDRESS
			+ conditionalPanelCopy_firstCell * conditionalPanelCopy_rows;
	byte* const screenMatrix = (byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS
			+ conditionalPanelCopy_firstCell;
	volatile byte* const colourRam = (volatile byte*)CONDITIONAL_PANEL_COPY__COLOUR_ADDRESS
			+ conditionalPanelCopy_firstCell;
	mmap_trampoline();
	const char previousMemoryMap = mmap_set(MMAP_NO_ROM);
	if (shouldRestore)
	{
		memcpy(bitmap, conditionalPanelCopy_background.bitmap,
				sizeof(conditionalPanelCopy_background.bitmap));
		memcpy(screenMatrix, conditionalPanelCopy_background.screenMatrix,
				conditionalPanelCopy_columns);
	}
	else
	{
		memcpy(conditionalPanelCopy_background.bitmap, bitmap,
				sizeof(conditionalPanelCopy_background.bitmap));
		memcpy(conditionalPanelCopy_background.screenMatrix, screenMatrix,
				conditionalPanelCopy_columns);
	}
	for (uint8_t column = 0u; column < conditionalPanelCopy_columns; ++column)
	{
		if (shouldRestore)
			colourRam[column] = conditionalPanelCopy_background.colourRam[column];
		else
			conditionalPanelCopy_background.colourRam[column] = colourRam[column];
	}
	mmap_set(previousMemoryMap);
}

static __noinline void storeAbsolute(byte value)
{
	*(byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS = value;
}

static __noinline void fillAbsolute(byte value)
{
	memset((byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS, value, 11u);
}

static __noinline byte loadAbsolute(void)
{
	return *(byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS;
}

static __noinline byte loadAbsoluteIndexed(byte index)
{
	return ((byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS)[index];
}

static void checkAbsoluteCalls(void)
{
	byte* const screen = (byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS;
	screen[0] = 0;
	storeAbsolute(0x39);
	assert(screen[0] == 0x39);
	screen[0] = 0;
	fillAbsolute(0x52);
	for (byte i = 0; i < 11; ++i)
		assert(screen[i] == 0x52);
	screen[0] = 0x63;
	assert(loadAbsolute() == 0x63);
	screen[0] = 0x74;
	screen[1] = 0x85;
	assert(loadAbsoluteIndexed(0) == 0x74);
	assert(loadAbsoluteIndexed(1) == 0x85);
}

/**
 * @brief Checks that calls invalidate cached values at absolute addresses.
 * @return Zero when the restored screen is read again after the copy call.
 */
int main(void)
{
	mmap_set(MMAP_NO_BASIC);
	byte* const screenMatrix = (byte*)CONDITIONAL_PANEL_COPY__SCREEN_ADDRESS
			+ conditionalPanelCopy_firstCell;
	memset(screenMatrix, conditionalPanelCopy_pattern, conditionalPanelCopy_columns);
	conditionalPanelCopy_preserveBackground(false);
	memset(screenMatrix, 0u, conditionalPanelCopy_columns);
	conditionalPanelCopy_preserveBackground(true);
	assert(screenMatrix[0u] == conditionalPanelCopy_pattern);
	for (byte column = 0; column < conditionalPanelCopy_columns; ++column)
		assert(screenMatrix[column] == conditionalPanelCopy_pattern);
	checkAbsoluteCalls();
	return 0;
}
