#include <assert.h>
#include <stdint.h>

struct Position
{
	uint16_t column;
	uint16_t row;
};

union Coordinate
{
	uint16_t value;
	uint8_t byte;
};

static volatile uint8_t calls;

__noinline const struct Position *nextPosition(void)
{
	static const struct Position position = {0x1234u, 0x5678u};
	++calls;
	return &position;
}

int main(void)
{
	// Original reproducer: each expression initialises one complete element.
	const struct Position position = {1u, 2u};
	const struct Position positions[] = {position, position};
	assert(sizeof(positions) == 2u * sizeof(position));
	for (uint8_t i = 0; i < 2u; ++i)
	{
		assert(positions[i].column == 1u);
		assert(positions[i].row == 2u);
	}

	struct Position source = {300u, 700u};
	const struct Position *pointer = &source;
	const struct Position mixed[] = {*pointer, {11u, 22u}, positions[1], *nextPosition()};
	source.column = 999u;
	source.row = 888u;
	assert(sizeof(mixed) == 4u * sizeof(position));
	assert(mixed[0].column == 300u && mixed[0].row == 700u);
	assert(mixed[1].column == 11u && mixed[1].row == 22u);
	assert(mixed[2].column == 1u && mixed[2].row == 2u);
	assert(mixed[3].column == 0x1234u && mixed[3].row == 0x5678u);
	assert(calls == 1u);

	struct Position grid[2][2] = {{position, source}, {mixed[1], position}};
	assert(grid[0][0].column == 1u && grid[0][0].row == 2u);
	assert(grid[0][1].column == 999u && grid[0][1].row == 888u);
	assert(grid[1][0].column == 11u && grid[1][0].row == 22u);
	assert(grid[1][1].column == 1u && grid[1][1].row == 2u);
	grid[1][1].column = 42u;
	assert(position.column == 1u);

	union Coordinate coordinate = {0x3456u};
	union Coordinate coordinates[] = {coordinate, {0x789au}, coordinate};
	assert(sizeof(coordinates) == 3u * sizeof(coordinate));
	assert(coordinates[0].value == 0x3456u);
	assert(coordinates[1].value == 0x789au);
	assert(coordinates[2].value == 0x3456u);

	// Brace elision must still consume scalars as individual members.
	struct Position flat[] = {1u, 2u, 3u, 4u};
	struct Position dynamic[] = {source.column, source.row, 5u, 6u};
	assert(sizeof(flat) == 2u * sizeof(position));
	assert(flat[0].column == 1u && flat[0].row == 2u);
	assert(flat[1].column == 3u && flat[1].row == 4u);
	assert(sizeof(dynamic) == 2u * sizeof(position));
	assert(dynamic[0].column == 999u && dynamic[0].row == 888u);
	assert(dynamic[1].column == 5u && dynamic[1].row == 6u);
	return 0;
}
