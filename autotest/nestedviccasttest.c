/* Casts must consume the complete operand, including its postfix operators. */
#include <assert.h>
#include <stdint.h>
#include <c64/vic.h>

struct Positions
{
	struct Position
	{
		volatile uint8_t x, y;
	} spr_pos[8];
};

static __noinline uint8_t readColumn(void)
{
	return vic.spr_pos[7].x;
}

int main(void)
{
	const uint8_t spriteIndex = 7u;
	struct Positions positions;
	struct Positions *pointer = &positions;
	uint8_t values[2] = {200u, 201u};
	uint8_t index = 0u;
	uint8_t counter = 200u;

	vic.spr_pos[spriteIndex].x = 200u;
	vic.spr_pos[spriteIndex].y = 201u;
	const uint16_t column = (uint16_t)vic.spr_pos[spriteIndex].x;
	const int16_t row = (int16_t)vic.spr_pos[spriteIndex].y;
	assert(column == 200u);
	assert(row == 201);

	/* Retain the former workaround as an equivalent syntax control. */
	assert(column == (uint16_t)(vic.spr_pos[spriteIndex].x));
	assert(row == (int16_t)(vic.spr_pos[spriteIndex].y));

	positions.spr_pos[spriteIndex].x = 200u;
	positions.spr_pos[spriteIndex].y = 201u;
	assert((uint16_t)(positions).spr_pos[spriteIndex].x == 200u);
	assert((int16_t)(pointer)->spr_pos[spriteIndex].y == 201);
	assert((uint16_t)(*pointer).spr_pos[spriteIndex].x == 200u);
	assert((int16_t)((struct Positions *)pointer)->spr_pos[spriteIndex].y == 201);

	/* Indexing, calls and increments bind before the cast as well. */
	assert((uint16_t)(values)[index++] == 200u);
	assert(index == 1u);
	assert((int16_t)(readColumn)() == 200);
	assert((uint16_t)(counter)++ == 200u);
	assert(counter == 201u);
	assert((int16_t)(uint8_t)(counter) == 201);
	assert((int16_t)-(counter) == -201);
	assert((int16_t)(index++, counter) == 201);
	assert(index == 2u);
	return 0;
}
