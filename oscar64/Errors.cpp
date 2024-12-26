#include "Errors.h"
#include "Ident.h"
#include <stdio.h>
#include <stdlib.h>

Errors::Errors(void)
	: mErrorCount(0)
{

}

void Errors::Error(const Location& loc, ErrorID eid, const char* msg, const Ident* info1, const Ident* info2)
{
	if (!info1)
		this->Error(loc, eid, msg);
	else if (!info2)
		this->Error(loc, eid, msg, info1->mString);
	else
		this->Error(loc, eid, msg, info1->mString, info2->mString);
}


void Errors::Error(const Location& loc, ErrorID eid, const char* msg, const char* info1, const char * info2) 
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

	if (loc.mFileName)
	{
		if (!info1)
			fprintf(stderr, "%s(%d, %d) : %s %d: %s\n", loc.mFileName, loc.mLine, loc.mColumn, level, eid, msg);
		else if (!info2)
			fprintf(stderr, "%s(%d, %d) : %s %d: %s '%s'\n", loc.mFileName, loc.mLine, loc.mColumn, level, eid, msg, info1);
		else
			fprintf(stderr, "%s(%d, %d) : %s %d: %s '%s' != '%s'\n", loc.mFileName, loc.mLine, loc.mColumn, level, eid, msg, info1, info2);

		if (loc.mFrom)
			Error(*(loc.mFrom), EINFO_EXPANDED, "While expanding here");
	}
	else
	{
		if (!info1)
			fprintf(stderr, "oscar64: %s %d: %s\n", level, eid, msg);
		else if (!info2)
			fprintf(stderr, "oscar64: %s %d: %s '%s'\n", level, eid, msg, info1);
		else
			fprintf(stderr, "oscar64: %s %d: %s '%s' != '%s'\n", level, eid, msg, info1, info2);
	}
	
	if (mErrorCount > 10 || eid >= EFATAL_GENERIC)
		exit(20);
}


