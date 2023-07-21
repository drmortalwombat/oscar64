#ifndef OPP_OFSTREAM_H
#define OPP_OFSTREAM_H

#include "iostream.h"
#include "string.h"


class ofstream : public ostream
{
public:
	ofstream(char fnum, char device, char channel, const string & name);
	~ofstream(void);

protected:
	virtual void bput(char ch);

	char	mBuffer[32];
	char	mBufferFill;

	char fnum;
};



#pragma compile("ofstream.cpp")

#endif
