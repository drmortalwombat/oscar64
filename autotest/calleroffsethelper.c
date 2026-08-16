#include "calleroffsethelper.h"

#include <stdint.h>
#include <stdlib.h>

__noinline long selectWithinRange(long minimum, long maximum)
{
	return minimum + rand() / (RAND_MAX / (maximum - minimum + 1) + 1);
}

inline char selectByte(char minimum, char maximum)
{
	return (char)selectWithinRange(minimum, maximum);
}
