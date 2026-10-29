/*
 * Regression test for global struct snapshot value forwarding across function calls.
 *
 * When an automatic local struct is initialised from a global struct and a subsequent
 * function call modifies the global struct, reads from the local struct snapshot
 * must not be incorrectly forwarded to the modified global struct.
 */

struct Position
{
	unsigned char column;
	unsigned char row;
};

enum
{
	initialCoordinate = 0u,
	movedColumn = 1u
};

static struct Position globalPosition = {
	initialCoordinate,
	initialCoordinate
};

static __noinline void movePlayer(void)
{
	globalPosition.column = movedColumn;
}

int main(void)
{
	const struct Position previousPosition = globalPosition;

	movePlayer();

	return (
		previousPosition.column != globalPosition.column
		|| previousPosition.row != globalPosition.row
	) ? 0 : 1;
}
