/*
 * Regression test for optimiser warning 2007 when a small signed structure
 * returned by a function is used in wider co-ordinate arithmetic. The
 * generated X/Y transfer ranges used to move an immediate logic operation in
 * opposite directions and prevented the optimiser from reaching a fixed point.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

struct CoordinateInt8
{
	int8_t column;
	int8_t row;
};

struct CoordinateInt16
{
	int16_t column;
	int16_t row;
};

static uint8_t screenColumn;
static uint8_t screenRow;
static struct CoordinateInt16 position;
static uint8_t speed = 1;

static struct CoordinateInt8 makeCoordinateInt8(const int8_t column, const int8_t row)
{
	return (struct CoordinateInt8){column, row};
}

static bool moveTest(void)
{
	const struct CoordinateInt8 velocityVector = makeCoordinateInt8(
		(int8_t)speed,
		(int8_t)speed
	);

	if (velocityVector.column == 0 && velocityVector.row == 0)
		return speed != 0;
	const struct CoordinateInt16 previousPosition = position;
	bool didEnterScreen = false;
	struct CoordinateInt16 nextPosition = {
		position.column + velocityVector.column,
		position.row + velocityVector.row
	};

	if (nextPosition.column < 24)
	{
		if (screenColumn > 0)
		{
			--screenColumn;
			nextPosition.column = 320;
			didEnterScreen = true;
		}
		else
			nextPosition.column = 24;
	}
	else if (nextPosition.column > 320)
	{
		if (screenColumn < 2)
		{
			++screenColumn;
			nextPosition.column = 24;
			didEnterScreen = true;
		}
		else
			nextPosition.column = 320;
	}

	if (nextPosition.row < 50)
	{
		if (screenRow > 0)
		{
			--screenRow;
			nextPosition.row = 229;
			didEnterScreen = true;
		}
		else
			nextPosition.row = 50;
	}
	else if (nextPosition.row > 229)
	{
		if (screenRow < 2)
		{
			++screenRow;
			nextPosition.row = 50;
			didEnterScreen = true;
		}
		else
			nextPosition.row = 229;
	}

	if (!didEnterScreen)
	{
		if (
			nextPosition.column == previousPosition.column &&
			nextPosition.row == previousPosition.row
		)
			return false;
	}
	position = nextPosition;
	return true;
}

int main(void)
{
	assert(moveTest());
	return 0;
}
