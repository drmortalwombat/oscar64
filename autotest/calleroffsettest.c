#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

__noinline long selectWithinRange(long minimum, long maximum)
{
	return minimum + rand() / (RAND_MAX / (maximum - minimum + 1) + 1);
}

inline char selectByte(char minimum, char maximum)
{
	return (char)selectWithinRange(minimum, maximum);
}

struct Position
{
	int column;
	int row;
};

struct Item
{
	struct Position position;
	unsigned char speed;
	char heading;
	unsigned char acceleration;
	char accelerationHeading;
	unsigned velocityRemainder;
	unsigned displacementRemainder;
	unsigned lastUpdate;
	struct Position displayedPosition;
	unsigned age;
	unsigned delay;
	unsigned char completedCount;
	unsigned char hasClearedSource;
	unsigned char isActive;
};

static struct Position sourcePosition;
static struct Item items[4];
static unsigned char processed[4];
static const char selections[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

/* Keep a library-capable call in the graph so the caller reserves a sizeable
 * volatile temporary area. */
__noinline void recordItem(unsigned char index)
{
	if (processed[index] == 255)
		printf("Unexpected item state\n");
	processed[index] = 1;
}

/* The leading arguments make the run-time index arrive through the software
 * stack, preserving the register-allocation shape which exposed the fault. */
static void initialiseItem(long first, long second, long third, long fourth,
	unsigned char index)
{
	(void)first;
	(void)second;
	(void)third;
	(void)fourth;
	struct Item * item = &items[index];
	item->position = sourcePosition;
	item->speed = 24;
	item->heading = selections[(unsigned char)selectByte(0, 7)];
	item->acceleration = 0;
	item->accelerationHeading = 0;
	item->velocityRemainder = 0;
	item->displacementRemainder = 0;
	item->lastUpdate = 0;
	item->displayedPosition = item->position;
	item->age = 0;
	item->delay = 0;
	item->completedCount = 0;
	item->hasClearedSource = 0;
	item->isActive = 1;
	recordItem(index);
}

int main(void)
{
	sourcePosition.column = 300;
	sourcePosition.row = 200;
	srand(1);
	initialiseItem(0, 0, 0, 0, 1);

	return items[1].displayedPosition.column != 300
		|| items[1].displayedPosition.row != 200
		|| ! processed[1];
}