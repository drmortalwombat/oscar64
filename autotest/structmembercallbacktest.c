// Regression: retain callback types when folding direct global struct members.
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum StructMemberCallbackDirection
{
	structMemberCallback_none,
	structMemberCallback_east
};

enum
{
	structMemberCallback_expectedTicks = 1u
};

/** @brief Advances one tick of the callback owner's playback state. */
typedef void (*StructMemberCallbackAdvanceFunction)(void);
/** @brief Samples held input. @return The current compass direction. */
typedef enum StructMemberCallbackDirection
		(*StructMemberCallbackSampleFunction)(void);

/** @brief Groups playback and input providers with their observable state. */
struct StructMemberCallbackState
{
	uint8_t ticks;
	bool isSampling;
	StructMemberCallbackAdvanceFunction advanceTick;
	StructMemberCallbackSampleFunction sampleDirection;
};

static struct StructMemberCallbackState structMemberCallback_state;

// Cover folded references through nested structs and constant array indices too.
static struct
{
	uint8_t padding;
	struct StructMemberCallbackState state;
} nested;
static struct StructMemberCallbackState states[2];
static struct
{
	uint8_t padding;
	void (*advance)(void);
	enum StructMemberCallbackDirection (*sample)(void);
} explicitCallbacks;

static void structMemberCallback_advanceTick(void)
{
	++structMemberCallback_state.ticks;
}

static enum StructMemberCallbackDirection structMemberCallback_sampleDirection(void)
{
	return structMemberCallback_east;
}

/**
 * @brief Exercises assignment, comparison and invocation with both callback return types.
 * @return Zero after exactly one playback tick and an eastward input sample.
 */
int main(void)
{
	structMemberCallback_state.isSampling = true;
	structMemberCallback_state.advanceTick = structMemberCallback_advanceTick;
	structMemberCallback_state.sampleDirection = structMemberCallback_sampleDirection;
	if (structMemberCallback_state.advanceTick != NULL)
		structMemberCallback_state.advanceTick();
	enum StructMemberCallbackDirection direction = structMemberCallback_none;
	if (structMemberCallback_state.isSampling
			&& structMemberCallback_state.sampleDirection != NULL)
		direction = structMemberCallback_state.sampleDirection();
	assert(structMemberCallback_state.ticks == structMemberCallback_expectedTicks);
	assert(direction == structMemberCallback_east);

	nested.state.advanceTick = structMemberCallback_advanceTick;
	nested.state.sampleDirection = structMemberCallback_sampleDirection;
	assert(nested.state.advanceTick != NULL);
	nested.state.advanceTick();
	assert(nested.state.sampleDirection() == structMemberCallback_east);

	states[1].advanceTick = structMemberCallback_advanceTick;
	states[1].sampleDirection = structMemberCallback_sampleDirection;
	states[1].advanceTick();
	assert(states[1].sampleDirection() == structMemberCallback_east);

	explicitCallbacks.advance = structMemberCallback_advanceTick;
	explicitCallbacks.sample = structMemberCallback_sampleDirection;
	explicitCallbacks.advance();
	assert(explicitCallbacks.sample() == structMemberCallback_east);
	assert(structMemberCallback_state.ticks == 4);

	// Keep the formerly passing local and arrow forms as controls.
	struct StructMemberCallbackState local;
	local.advanceTick = structMemberCallback_advanceTick;
	local.sampleDirection = structMemberCallback_sampleDirection;
	local.advanceTick();
	assert(local.sampleDirection() == structMemberCallback_east);
	(&structMemberCallback_state)->advanceTick();
	assert((&structMemberCallback_state)->sampleDirection() == structMemberCallback_east);
	assert(structMemberCallback_state.ticks == 6);

	structMemberCallback_state.advanceTick = NULL;
	structMemberCallback_state.sampleDirection = NULL;
	assert(structMemberCallback_state.advanceTick == NULL);
	assert(structMemberCallback_state.sampleDirection == NULL);
	return 0;
}
