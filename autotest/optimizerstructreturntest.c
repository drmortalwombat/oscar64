/*
 * Regression test for optimiser warning 2007 when a returned signed structure
 * is widened. The X/Y transfer rules used to prevent a fixed point.
 */
#pragma warning(error: 2007)

#include <stdint.h>

typedef struct { int8_t x, y; } Vector;
typedef struct { int16_t x, y; } Position;

static uint8_t screen;
static Position position;
static uint8_t speed = 1;

static Vector vector(int8_t x, int8_t y)
{
	return (Vector){x, y};
}

static int move(void)
{
	Vector velocity = vector(speed, speed);
	if (velocity.x == 0)
		return speed != 0;
	int entered = 0;
	Position next = {
		velocity.x,
		position.y + velocity.y
	};

	if (!next.x && screen)
		entered = 1;
	if (!next.y)
		next.y = 2;
	else if (next.y > 2)
		next.y = screen < 2 ? 1 : 2;
	if (!entered && next.y == position.y)
		return 0;
	return 1;
}

int main(void)
{
	return !move();
}
