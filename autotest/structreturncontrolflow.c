#include <stdbool.h>
#include <stdint.h>

enum Item
{
	itemNone,
	itemFood,
	itemWater
};

enum ResultKind
{
	resultNone,
	resultWarning,
	resultConsumed,
	resultDamaged
};

struct Result
{
	enum ResultKind result;
	uint8_t remaining;
	enum Item item;
	uint8_t damage;
};

struct Schedule
{
	uint8_t food;
	uint8_t water;
};

struct Player
{
	struct Schedule schedule;
	uint8_t health;
};

static enum Item consumeItem(enum Item inventory[], const uint8_t inventorySize, const bool isFood)
{
	if (inventorySize == 0u)
		return itemNone;

	const enum Item expectedItem = isFood ? itemFood : itemWater;
	if (inventory[0] != expectedItem)
		return itemNone;

	inventory[0] = itemNone;
	return expectedItem;
}

static uint8_t applyDamage(struct Player * const player, const uint8_t severity)
{
	const uint8_t damage = 5u + severity;
	player->health -= damage;
	return damage;
}

static struct Result processItem(
	struct Player * const player,
	enum Item inventory[],
	const uint8_t inventorySize,
	const uint16_t consumedHours,
	const uint8_t severity,
	const bool isFood)
{
	struct Result notice = {resultNone, 0u, itemNone, 0u};
	uint8_t * const hoursUntilConsumption = isFood
		? &player->schedule.food
		: &player->schedule.water;

	if (*hoursUntilConsumption <= consumedHours)
		*hoursUntilConsumption = 0u;
	else
		*hoursUntilConsumption -= (uint8_t)consumedHours;
	notice.remaining = *hoursUntilConsumption;

	if (notice.remaining > 18u)
		return notice;

	if (notice.remaining != 0u)
	{
		notice.result = resultWarning;
		return notice;
	}

	notice.item = consumeItem(inventory, inventorySize, isFood);
	notice.result = notice.item == itemNone ? resultDamaged : resultConsumed;

	if (notice.result == resultDamaged)
		notice.damage = applyDamage(player, severity);
	*hoursUntilConsumption = isFood ? 208u : 120u;
	return notice;
}

int main(void)
{
	struct Player player = {{0u, 0u}, 100u};
	enum Item inventory[1] = {itemFood};
	const struct Result notice = processItem(&player, inventory, 1u, 1u, 0u, true);

	return notice.result != resultConsumed
		|| notice.remaining != 0u
		|| notice.item != itemFood
		|| notice.damage != 0u
		|| player.schedule.food != 208u;
}
