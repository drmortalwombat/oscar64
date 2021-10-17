#include "Errors.h"
#include <stdio.h>
#include <stdlib.h>

Errors::Errors(void)
	: mErrorCount(0)
{

}

void Errors::Error(const Location& loc, ErrorID eid, const char* msg, const char* info) 
{
	const char* level = "info";
	if (eid >= EERR_GENERIC)
	{
		level = "error";
		mErrorCount++;
	}
	else if (eid >= EWARN_GENERIC)
	{
		level = "warning";
	}

	if (info)
		printf("%s(%d, %d) : %s %d: %s '%s'\n", loc.mFileName, loc.mLine, loc.mColumn, level ,eid, msg, info);
	else
		printf("%s(%d, %d) : %s %d: %s\n", loc.mFileName, loc.mLine, loc.mColumn, level, eid, msg);

	if (mErrorCount > 10)
		exit(20);
}



