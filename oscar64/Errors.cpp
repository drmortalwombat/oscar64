#include "Errors.h"
#include <stdio.h>
#include <stdlib.h>

Errors::Errors(void)
	: mErrorCount(0)
{

}

void Errors::Warning(const Location& loc, const char* error)
{
	printf("Warning %s in %s: %d, %d\n", error, loc.mFileName, loc.mLine, loc.mColumn);
}

void Errors::Error(const Location& loc, const char* error)
{
	printf("Error %s in %s: %d, %d\n", error, loc.mFileName, loc.mLine, loc.mColumn);
	mErrorCount++;
	if (mErrorCount > 10)
		exit(10);
}

void Errors::Error(const Location& loc, const char* error, const char* info)
{
	printf("Error %s '%s' in %s: %d, %d\n", error, info, loc.mFileName, loc.mLine, loc.mColumn);
	mErrorCount++;
	if (mErrorCount > 10)
		exit(10);
}



