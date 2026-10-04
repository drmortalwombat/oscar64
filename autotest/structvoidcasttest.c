/*
 * Regression for error 3011 when discarding a struct or union with a void cast.
 * Evaluate the operand's side effects without loading the aggregate as a scalar.
 */

#include <assert.h>

/** Coordinates passed by value in the original reproducer. */
struct Position
{
	unsigned short column;
	unsigned short row;
};

/** Larger coordinates exercise struct returns through memory. */
struct LargePosition
{
	unsigned long column;
	unsigned long row;
};

/** Union operands also have no scalar intermediate type. */
union Coordinate
{
	unsigned short column;
	unsigned short row;
};

static unsigned short structVoidCast_callCount;

static int discardPosition(const struct Position position)
{
	(void)position;
	return 0;
}

static __noinline struct Position createPosition(void)
{
	const struct Position position = {17u, 29u};
	structVoidCast_callCount++;
	return position;
}

static __noinline struct LargePosition createLargePosition(void)
{
	const struct LargePosition position = {170000ul, 290000ul};
	structVoidCast_callCount++;
	return position;
}

static __noinline void recordCall(void)
{
	structVoidCast_callCount++;
}

/**
 * Exercise discarded aggregates and verify their operand side effects.
 * @return Zero when all assertions pass.
 */
int main(void)
{
	const struct Position position = {17u, 29u};
	struct Position assignedPosition = {0u, 0u};
	const struct Position positions[2] = {{17u, 29u}, {31u, 43u}};
	const struct Position *positionPointer = positions;
	const struct LargePosition largePosition = {170000ul, 290000ul};
	union Coordinate coordinate;
	unsigned short selected = 0u;
	volatile unsigned short scalar = 17u;

	assert(discardPosition(position) == 0);
	(void)position;
	(void)largePosition;
	(void)*positionPointer++;
	assert(positionPointer == positions + 1);

	coordinate.column = 17u;
	(void)coordinate;
	(void)(coordinate.column = 29u, coordinate);
	assert(coordinate.column == 29u);

	(void)createPosition();
	assert(structVoidCast_callCount == 1u);
	(void)createLargePosition();
	assert(structVoidCast_callCount == 2u);

	(void)(assignedPosition = position);
	assert(assignedPosition.column == 17u);
	assert(assignedPosition.row == 29u);

	(void)(selected++ ? createPosition() : position);
	assert(selected == 1u);
	assert(structVoidCast_callCount == 2u);
	(void)(selected++ ? createPosition() : position);
	assert(selected == 2u);
	assert(structVoidCast_callCount == 3u);

	(void)(void)position;
	(void)recordCall();
	assert(structVoidCast_callCount == 4u);
	(void)scalar;
	(void)(scalar += 2u);
	assert(scalar == 19u);
	return 0;
}
