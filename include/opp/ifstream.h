#ifndef OPP_IFSTREAM_H
#define OPP_IFSTREAM_H

#include "iostream.h"
#include "string.h"

namespace opp {

class ifstream : public istream
{
public:
	ifstream(char fnum, char device, char channel, const string & name);
	~ifstream(void);

protected:
	virtual void refill(void);

	char fnum;
};

}

#pragma compile("ifstream.cpp")

#endif