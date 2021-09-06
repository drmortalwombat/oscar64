#pragma once

class Location
{
public:
	const char* mFileName;
	int			mLine, mColumn;

	Location() : mFileName(nullptr), mLine(0), mColumn(0) {}
};


class Errors
{
public:
	Errors(void);

	int		mErrorCount;

	void Error(const Location& loc, const char* msg);
	void Error(const Location& loc, const char* msg, const char * info);
	void Warning(const Location& loc, const char* msg);
};
