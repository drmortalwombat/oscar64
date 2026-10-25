/**
 * @file
 * @brief Reproduces a native conditional memcpy losing a calculated pointer's low byte.
 *
 * A local const uint16_t offset is added to three fixed C64 display addresses before saving and
 * restoring a panel.
 *
 * The final assertion must pass: the screen byte at 0xCBC0 must return to 0x26 after restoration.
 * Before the fix, native compilation with emulator assertions failed that postcondition.
 * The pointer-folding check missed indirect accesses overlapping the candidate register pair.
 */

#include <c64/memmap.h>
#include <c64/types.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define LOCAL_OFFSET_PANEL_COPY__BITMAP_ADDRESS (0xE000u)
#define LOCAL_OFFSET_PANEL_COPY__SCREEN_ADDRESS (0xC800u)
#define LOCAL_OFFSET_PANEL_COPY__COLOUR_ADDRESS (0xD800u)

enum
{
	localOffsetPanelCopy_columns = 11u,
	localOffsetPanelCopy_rows = 8u,
	localOffsetPanelCopy_firstCell = 960u,
	localOffsetPanelCopy_pattern = 0x26u
};

/**
 * @brief Groups the saved bitmap and palette planes for a conditional copy.
 */
struct LocalOffsetPanelCopyBackground
{
	byte bitmap[localOffsetPanelCopy_columns * localOffsetPanelCopy_rows];
	byte screenMatrix[localOffsetPanelCopy_columns];
	byte colourRam[localOffsetPanelCopy_columns];
};

static struct LocalOffsetPanelCopyBackground localOffsetPanelCopy_background;

static void localOffsetPanelCopy_preserveBackground(const bool shouldRestore)
{
	// A local offset retains the pointer calculation used by the game's panel copies.
	const uint16_t firstCell = localOffsetPanelCopy_firstCell;
	byte* const bitmap = (byte*)LOCAL_OFFSET_PANEL_COPY__BITMAP_ADDRESS
			+ firstCell * localOffsetPanelCopy_rows;
	byte* const screenMatrix = (byte*)LOCAL_OFFSET_PANEL_COPY__SCREEN_ADDRESS
			+ firstCell;
	volatile byte* const colourRam = (volatile byte*)LOCAL_OFFSET_PANEL_COPY__COLOUR_ADDRESS
			+ firstCell;
	mmap_trampoline();
	const char previousMemoryMap = mmap_set(MMAP_NO_ROM);

	if (shouldRestore)
	{
		memcpy(bitmap, localOffsetPanelCopy_background.bitmap,
				sizeof(localOffsetPanelCopy_background.bitmap));
		memcpy(screenMatrix, localOffsetPanelCopy_background.screenMatrix,
				localOffsetPanelCopy_columns);
	}
	else
	{
		memcpy(localOffsetPanelCopy_background.bitmap, bitmap,
				sizeof(localOffsetPanelCopy_background.bitmap));
		memcpy(localOffsetPanelCopy_background.screenMatrix, screenMatrix,
				localOffsetPanelCopy_columns);
	}

	for (uint8_t column = 0u ; column < localOffsetPanelCopy_columns ; ++column)
	{
		if (shouldRestore)
			colourRam[column] = localOffsetPanelCopy_background.colourRam[column];
		else
			localOffsetPanelCopy_background.colourRam[column] = colourRam[column];
	}
	mmap_set(previousMemoryMap);
}

/**
 * @brief Checks that a conditional native copy restores the screen matrix at its exact address.
 * @return Zero when the screen pointer retains its low address byte through both copy branches.
 */
int main(void)
{
	// Given a known screen-matrix pattern at a non-page-aligned address.
	mmap_set(MMAP_NO_BASIC);
	byte* const screenMatrix = (byte*)LOCAL_OFFSET_PANEL_COPY__SCREEN_ADDRESS
			+ localOffsetPanelCopy_firstCell;
	memset(screenMatrix, localOffsetPanelCopy_pattern, localOffsetPanelCopy_columns);
	// When the panel is saved, overwritten and restored through the same conditional copies.
	localOffsetPanelCopy_preserveBackground(false);
	memset(screenMatrix, 0u, localOffsetPanelCopy_columns);
	localOffsetPanelCopy_preserveBackground(true);

	// Then the saved pattern must be back at the original address, not elsewhere in its page.
	for (uint8_t column = 0u ; column < localOffsetPanelCopy_columns ; ++column)
		assert(screenMatrix[column] == localOffsetPanelCopy_pattern);
	return 0;
}
