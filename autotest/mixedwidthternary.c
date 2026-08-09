#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum
{
	healthDamageDivisor = 5u,
	playerMaximumHealth = 17u,
	expectedHealthDamage = 3u,
	expectedRemainingHealth = playerMaximumHealth - expectedHealthDamage
};

enum WareItem
{
	WARE_ITEM__NONE,
	WARE_ITEM__FOOD
};

struct Player
{
	uint8_t health;
	uint8_t maxHealth;
};

static uint16_t calculateExtraDamage(uint8_t severity, uint16_t startDamage)
{
	if (severity == 0u)
		return 0u;
	return startDamage;
}

static uint8_t damagePlayer(struct Player * player, uint8_t severity)
{
	uint16_t intendedDamage = player->maxHealth / healthDamageDivisor;
	intendedDamage += calculateExtraDamage(severity, intendedDamage);
	const uint8_t healthDamage = player->health < intendedDamage
		? player->health
		: intendedDamage;
	player->health -= healthDamage;
	return healthDamage;
}

static enum WareItem consumeProvision(enum WareItem inventory[])
{
	if (inventory == NULL)
		puts("Invalid inventory");

	if (inventory[0] == WARE_ITEM__FOOD)
	{
		inventory[0] = WARE_ITEM__NONE;
		return WARE_ITEM__FOOD;
	}
	return WARE_ITEM__NONE;
}

int main(void)
{
	struct Player player = { playerMaximumHealth, playerMaximumHealth };
	enum WareItem inventory[1] = { WARE_ITEM__NONE };
	consumeProvision(inventory);

	const uint8_t healthDamage = damagePlayer(&player, 0u);
	assert(healthDamage == expectedHealthDamage);
	assert(player.health == expectedRemainingHealth);
	return 0;
}
