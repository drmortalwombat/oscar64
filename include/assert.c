#include "assert.h"
#include <stdlib.h>

void assert(bool b)
{
	if (!b)
		exit(-1);
}
