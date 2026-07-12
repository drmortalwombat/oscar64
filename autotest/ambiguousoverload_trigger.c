#include "ambiguousoverload.h"

#include <assert.h>


void setup(void);

int clearViewCalls = 0;

// Kept in this companion translation unit to exercise cross-unit merging.
void clearView(void)
{
	clearViewCalls++;
}

int main(void)
{
	setup();
	assert(clearViewCalls == 2);
	// This direct call was incorrectly diagnosed as an ambiguous overload.
	clearView();
	assert(clearViewCalls == 3);
	return 0;
}
