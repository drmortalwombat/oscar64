/**
 * @file
 * @brief Reproduces native code losing a returned co-ordinate's row before a lookup.
 *
 * Retains the original failing call and data flow: the returned row is shared by a
 * parameter store and a local store before the visibility call.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum
{
	viewportStructReturn_columns = 90u,
	viewportStructReturn_rows = 80u,
	viewportStructReturn_cellCount = viewportStructReturn_columns * viewportStructReturn_rows,
	viewportStructReturn_municipalityColumn = 38u,
	viewportStructReturn_municipalityRow = 34u,
	viewportStructReturn_worldColumn = 24u,
	viewportStructReturn_worldRow = 26u,
	viewportStructReturn_viewportColumn = 14u,
	viewportStructReturn_viewportRow = 8u,
	viewportStructReturn_terrainMask = 0x0fu,
	viewportStructReturn_nybbleBitShift = 2u,
	viewportStructReturn_municipality = 0u,
	viewportStructReturn_glacier = 6u
};

/**
 * @brief Groups the column and row of one world location.
 */
struct WorldPosition
{
	uint16_t column;
	uint16_t row;
};

/**
 * @brief Groups the column and row of one viewport cell.
 */
struct ViewportPosition
{
	uint8_t column;
	uint8_t row;
};

static uint8_t viewportStructReturn_terrain[viewportStructReturn_cellCount / 2u];
static uint8_t viewportStructReturn_seen[viewportStructReturn_cellCount / 8u];

static struct WorldPosition viewportStructReturn_makePosition(
	const struct WorldPosition world,
	const struct ViewportPosition viewport
)
{
	const uint16_t unboundedRow = world.row + viewport.row;
	const uint16_t boundedRow = unboundedRow < viewportStructReturn_rows
			? unboundedRow
			: viewportStructReturn_rows - 1u;
	return (struct WorldPosition){
		(world.column + viewport.column) % viewportStructReturn_columns,
		boundedRow
	};
}

static inline uint16_t viewportStructReturn_locationIndex(const struct WorldPosition position)
{
	const uint16_t boundedRow = position.row < viewportStructReturn_rows
			? position.row
			: viewportStructReturn_rows - 1u;
	return boundedRow * viewportStructReturn_columns
			+ position.column % viewportStructReturn_columns;
}

static bool viewportStructReturn_isSeen(const struct WorldPosition position)
{
	const uint16_t locationIndex = viewportStructReturn_locationIndex(position);
	return (viewportStructReturn_seen[locationIndex >> 3u]
			& (1u << (locationIndex & 7u))) != 0u;
}

static uint8_t viewportStructReturn_selectTerrain(
	const struct ViewportPosition viewport,
	const struct WorldPosition world
)
{
	const struct WorldPosition position = viewportStructReturn_makePosition(world, viewport);
	if (viewportStructReturn_isSeen(position))
	{
		const uint16_t locationIndex = viewportStructReturn_locationIndex(position);
		const uint8_t shift = (uint8_t)(
			(locationIndex & 1u) << viewportStructReturn_nybbleBitShift
		);
		return (viewportStructReturn_terrain[locationIndex >> 1u] >> shift)
				& viewportStructReturn_terrainMask;
	}
	return UINT8_MAX;
}

/**
 * @brief Checks that a visible municipality is read from its actual row.
 * @return Zero when the terrain lookup preserves the returned co-ordinate.
 */
int main(void)
{
	// Given a visible municipality and a different terrain at the same column on row zero.
	const uint16_t municipalityIndex = viewportStructReturn_municipalityRow
			* viewportStructReturn_columns + viewportStructReturn_municipalityColumn;
	viewportStructReturn_terrain[viewportStructReturn_municipalityColumn / 2u] =
			viewportStructReturn_glacier;
	viewportStructReturn_seen[municipalityIndex >> 3u] = 1u << (municipalityIndex & 7u);
	const struct WorldPosition world = {
		viewportStructReturn_worldColumn,
		viewportStructReturn_worldRow
	};
	const struct ViewportPosition viewport = {
		viewportStructReturn_viewportColumn,
		viewportStructReturn_viewportRow
	};

	// When the returned position is used for visibility and terrain queries.
	const uint8_t terrain = viewportStructReturn_selectTerrain(viewport, world);

	// Then the selected terrain must be the municipality rather than the polar glacier.
	assert(terrain == viewportStructReturn_municipality);
	return 0;
}
