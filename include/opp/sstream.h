#ifndef OPP_SSTREAM_H
#define OPP_SSTREAM_H

#include "iostream.h"

namespace opp {

class ostringstream : public ostream
{
public:
	ostringstream(void);
	~ostringstream(void);

	string str(void) const;
	void str(const string & str);
protected:
	void bput(char ch);

	char	*	mBuffer;
	char		mBFill, mBSize;
};

class istringstream : public istream
{
public:
	istringstream(const string & str);
	~istringstream(void);

	string str(void) const;
	void str(const string & str);
protected:
	virtual void refill(void);

	string		mString;
	char		mSPos;
};

}

#pragma compile("sstream.cpp")

#endif
