/* Regression details and execution instructions: nestedcoordinatecopytest.txt. */
#include <c64/vic.h>
#include <assert.h>
#include <stdint.h>

enum
{
	nestedCoOrdinateCopies_sceneCount = 5u,
	nestedCoOrdinateCopies_mirrorCount = 2u,
	nestedCoOrdinateCopies_mirrorColumn = 344,
	nestedCoOrdinateCopies_spriteIndex = 0u
};

/**
 * @brief Keeps the signed components copied from each nested scene together.
 */
struct NestedCoOrdinateCopiesPosition
{
	int16_t column;
	int16_t row;
};

/**
 * @brief Preserves the scene-table stride and its two independently copied positions.
 */
struct NestedCoOrdinateCopiesScene
{
	uint8_t background;
	struct NestedCoOrdinateCopiesPosition front;
	struct NestedCoOrdinateCopiesPosition back;
};

static void nestedCoOrdinateCopies_moveSprite(const int16_t column, const int16_t row)
{
	vic.spr_pos[nestedCoOrdinateCopies_spriteIndex].x = (uint8_t)column;
	vic.spr_pos[nestedCoOrdinateCopies_spriteIndex].y = (uint8_t)row;
}

static void nestedCoOrdinateCopies_checkPosition(
	const struct NestedCoOrdinateCopiesPosition position
)
{
	assert(vic.spr_pos[nestedCoOrdinateCopies_spriteIndex].x == (uint8_t)position.column);
	assert((int16_t)vic.spr_pos[nestedCoOrdinateCopies_spriteIndex].y == position.row);
}

/**
 * @brief Checks structure copies passed by value within nested scene and mirror loops.
 *
 * @return Zero when every copied front and back position matches independent sprite writes.
 */
int main(void)
{
	static const struct NestedCoOrdinateCopiesScene scenes[nestedCoOrdinateCopies_sceneCount] =
	{
		{0u, {90, 189}, {34, 210}},
		{1u, {89, 90}, {24, 71}},
		{2u, {204, 219}, {150, 219}},
		{3u, {204, 53}, {150, 53}},
		{4u, {90, 136}, {28, 136}}
	};
	for (uint8_t sceneIndex = 0u; sceneIndex < nestedCoOrdinateCopies_sceneCount; ++sceneIndex)
	{
		for (uint8_t mirrorIndex = 0u; mirrorIndex < nestedCoOrdinateCopies_mirrorCount;
				++mirrorIndex)
		{
			// Copy both positions before either column is changed, as in the affected scene loop.
			struct NestedCoOrdinateCopiesPosition front = scenes[sceneIndex].front;
			struct NestedCoOrdinateCopiesPosition back = scenes[sceneIndex].back;
			if (mirrorIndex != 0u)
			{
				front.column = nestedCoOrdinateCopies_mirrorColumn - front.column;
				back.column = nestedCoOrdinateCopies_mirrorColumn - back.column;
			}
			// Keep sprite writes independent of the copied structures checked by value below.
			nestedCoOrdinateCopies_moveSprite(mirrorIndex != 0u
					? nestedCoOrdinateCopies_mirrorColumn - scenes[sceneIndex].front.column
					: scenes[sceneIndex].front.column, scenes[sceneIndex].front.row);
			nestedCoOrdinateCopies_checkPosition(front);
			nestedCoOrdinateCopies_moveSprite(mirrorIndex != 0u
					? nestedCoOrdinateCopies_mirrorColumn - scenes[sceneIndex].back.column
					: scenes[sceneIndex].back.column, scenes[sceneIndex].back.row);
			nestedCoOrdinateCopies_checkPosition(back);
		}
	}
	return 0;
}
