#include <c64/vic.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>


enum
{
	grid_layoutCount = 4u,
	grid_pairedColumns = 2u,
	grid_spriteWidth = 24,
	grid_spriteHeight = 21,
	grid_origin = 100u,
	grid_firstSprite = 2u,
	grid_columnMaximum = 511u,
	grid_rowMaximum = 255u
};

/**
 * @brief Selects the geometry table entry whose conditional offsets are under test.
 */
enum GridLayoutKind
{
	grid_horizontalPair,
	grid_alternateHorizontalPair,
	grid_square,
	grid_single
};

/**
 * @brief Holds one pixel position passed by value between construction and hardware placement.
 */
struct GridPosition
{
	int16_t column;
	int16_t row;
};

/**
 * @brief Keeps each layout's grid dimensions together for conditional cell placement.
 */
struct GridGeometry
{
	uint8_t columns;
	// Keep the table stride at two bytes: removing this field suppresses the defect.
	uint8_t rows;
};

/**
 * @brief Keeps a grid's origin, layout kind and allocated hardware sprites together.
 */
struct GridLayout
{
	struct GridPosition position;
	enum GridLayoutKind kind;
	uint8_t firstSprite;
	uint8_t spriteCount;
};

static const struct GridGeometry grid_geometries[grid_layoutCount] =
{
	{2u, 1u}, {2u, 1u}, {2u, 2u}, {1u, 1u}
};

static inline struct GridPosition grid_makePosition(
	const int16_t column, const int16_t row
)
{
	return (struct GridPosition){column, row};
}

static void grid_moveSprite(
	const uint8_t spriteIndex, const struct GridPosition position
)
{
	vic.spr_pos[spriteIndex].x = (uint8_t)position.column;
	vic.spr_pos[spriteIndex].y = (uint8_t)position.row;
}

static void grid_positionLayout(const struct GridLayout* const layout)
{
	assert(layout != NULL);
	assert(layout->kind < grid_layoutCount);
	const struct GridGeometry* const geometry = &grid_geometries[layout->kind];

	for (uint8_t cellIndex = 0u; cellIndex < layout->spriteCount; ++cellIndex)
	{
		const uint8_t spriteIndex = layout->firstSprite + cellIndex;
		const uint8_t mask = (uint8_t)(1u << spriteIndex);
		// Keep the inline structure return and visibility branch: both trigger wrong native code.
		const struct GridPosition position = grid_makePosition(
				layout->position.column
						+ (int16_t)((geometry->columns == grid_pairedColumns
								? cellIndex & 1u : 0u) * grid_spriteWidth),
				layout->position.row
						+ (int16_t)((geometry->columns == grid_pairedColumns
								? cellIndex / grid_pairedColumns : cellIndex)
								* grid_spriteHeight));
		if (position.column < 0 || position.column > grid_columnMaximum
				|| position.row < 0 || position.row > grid_rowMaximum)
			vic.spr_enable &= (uint8_t)~mask;
		else
		{
			grid_moveSprite(spriteIndex, position);
			vic.spr_enable |= mask;
		}
	}
}

/**
 * @brief Checks that the second grid cell is beside the first rather than below it.
 *
 * @return Zero when native conditional grid offsets preserve the two-column layout.
 */
int main(void)
{
	const struct GridLayout layout =
	{
		{grid_origin, grid_origin}, grid_alternateHorizontalPair,
		grid_firstSprite, grid_pairedColumns
	};
	grid_positionLayout(&layout);
	assert(vic.spr_pos[layout.firstSprite].x == grid_origin);
	assert(vic.spr_pos[layout.firstSprite].y == grid_origin);
	assert(vic.spr_pos[layout.firstSprite + 1u].x
			== grid_origin + grid_spriteWidth);
	assert(vic.spr_pos[layout.firstSprite + 1u].y == grid_origin);
	return 0;
}
