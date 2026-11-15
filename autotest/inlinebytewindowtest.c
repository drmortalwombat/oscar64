/**
 * @file
 * @brief Reproduces native inlining reusing address/index temporaries as artwork bytes.
 *
 * Each source contains just three artwork bytes. Both queries must return false:
 * expanded $C0 aligned by one pixel gives $81, or by seven pixels gives $60;
 * neither shares an opaque pixel with expanded $0C. Clipping cannot add contact.
 *
 * Keep the indexed loads, clipping calculations and inline expansion: simplifying
 * these can conceal the native branch-tail optimisation defect.
 */
#include <assert.h>
#include <c64/types.h>
#include <c64/vic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum
{
	inlineByteWindow_bitsPerByte = 8u,
	inlineByteWindow_width = 24u,
	inlineByteWindow_rowBytes = inlineByteWindow_width / inlineByteWindow_bitsPerByte,
	inlineByteWindow_firstPattern = 0xc0u,
	inlineByteWindow_secondPattern = 0x0cu,
	inlineByteWindow_colourMask = 0x0fu,
	inlineByteWindow_nearOffset = 1u,
	inlineByteWindow_farOffset = 7u
};

/** @brief Groups one three-byte artwork row and its runtime colour mode. */
struct InlineByteWindowArtwork
{
	const byte* bytes;
	bool isSingleColour;
};

/** @brief Records the common clipped interval in the two artwork blocks. */
struct InlineByteWindowRegion
{
	uint8_t firstColumn;
	uint8_t secondColumn;
	uint8_t width;
};

enum
{
	inlineByteWindow_multicolourByteLowBitMask = 0x55u
};

/** @brief Expands one multicolour byte without widening the arithmetic beyond eight bits. */
static inline byte inlineByteWindow_opaqueByte(const byte pixels, const bool singleColour)
{
	const byte lowBits = (pixels | (pixels >> 1u)) & inlineByteWindow_multicolourByteLowBitMask;
	return singleColour ? pixels : (byte)(lowBits | (lowBits << 1u));
}

/** @brief Checks one clipped row with the first artwork preceding the second artwork. */
static bool inlineByteWindow_didByteRowsCollide(const struct InlineByteWindowArtwork* const first,
		const struct InlineByteWindowArtwork* const second,
		const struct InlineByteWindowRegion* const region)
{
	// Anchor the windows to bytes of the later sprite. Only the other sprite needs shifts;
	// choosing the smaller clip offset keeps its first source column non-negative.
	const uint8_t rowBytes = inlineByteWindow_width / inlineByteWindow_bitsPerByte;
	const struct InlineByteWindowArtwork* const aligned = second;
	const struct InlineByteWindowArtwork* const shifted = first;
	const uint8_t alignedColumn = region->secondColumn;
	const uint8_t shiftedColumn = region->firstColumn;
	const byte* const alignedRow = aligned->bytes;
	const byte* const shiftedRow = shifted->bytes;
	const uint8_t leadingBits = alignedColumn % inlineByteWindow_bitsPerByte;
	const uint8_t alignedByte = alignedColumn / inlineByteWindow_bitsPerByte;
	const uint8_t shiftedStart = shiftedColumn - leadingBits;
	const uint8_t shiftedByte = shiftedStart / inlineByteWindow_bitsPerByte;
	const uint8_t shift = shiftedStart % inlineByteWindow_bitsPerByte;
	const uint8_t lastColumn = leadingBits + region->width - 1u;
	const uint8_t lastByte = lastColumn / inlineByteWindow_bitsPerByte;
	const byte firstMask = UINT8_MAX >> leadingBits;
	const byte lastMask = (byte)(UINT8_MAX << (inlineByteWindow_bitsPerByte - 1u
			- lastColumn % inlineByteWindow_bitsPerByte));
	for (uint8_t column = 0u; column <= lastByte; ++column)
	{
		byte alignedPixels = inlineByteWindow_opaqueByte(alignedRow[alignedByte + column],
				aligned->isSingleColour);
		if (column == 0u)
			alignedPixels &= firstMask;
		if (column == lastByte)
			alignedPixels &= lastMask;
		if (alignedPixels == 0u)
			continue;
		const uint8_t source = shiftedByte + column;
		byte shiftedPixels = (byte)(inlineByteWindow_opaqueByte(shiftedRow[source],
				shifted->isSingleColour) << shift);
		if (shift != 0u && source + 1u < rowBytes)
			shiftedPixels |= inlineByteWindow_opaqueByte(shiftedRow[source + 1u],
					shifted->isSingleColour) >> (inlineByteWindow_bitsPerByte - shift);
		if ((alignedPixels & shiftedPixels) != 0u)
			return true;
	}
	return false;
}

/**
 * @brief Verifies that disjoint multicolour stripes remain disjoint after alignment.
 * @return Zero if both queries preserve the loaded byte while expanding its colour pairs.
 */
int main(void)
{
	byte firstBytes[inlineByteWindow_rowBytes];
	byte secondBytes[inlineByteWindow_rowBytes];
	memset(firstBytes, inlineByteWindow_firstPattern, sizeof(firstBytes));
	memset(secondBytes, inlineByteWindow_secondPattern, sizeof(secondBytes));
	// Retain runtime mode branches with volatile hardware reads, rather than test-only globals.
	vic.color_border = 0u;
	const struct InlineByteWindowArtwork first = {firstBytes,
		(vic.color_border & inlineByteWindow_colourMask) != 0u};
	const struct InlineByteWindowArtwork second = {secondBytes,
		(vic.color_border & inlineByteWindow_colourMask) != 0u};
	const struct InlineByteWindowRegion nearRegion = {inlineByteWindow_nearOffset, 0u,
		inlineByteWindow_width - inlineByteWindow_nearOffset};
	const struct InlineByteWindowRegion farRegion = {inlineByteWindow_farOffset, 0u,
		inlineByteWindow_width - inlineByteWindow_farOffset};
	const bool nearContact = inlineByteWindow_didByteRowsCollide(&first, &second, &nearRegion);
	const bool farContact = inlineByteWindow_didByteRowsCollide(&first, &second, &farRegion);
	assert( ! nearContact);
	assert( ! farContact);
	return 0;
}
