#include "sstream.h"
#include <stdlib.h>

namespace opp {

ostringstream::ostringstream(void)
{
	mBuffer = nullptr;
	mBFill = mBSize = 0;
}

ostringstream::~ostringstream(void)
{
	free(mBuffer);
}

void ostringstream::bput(char ch)
{
	if (!mBuffer)
	{
		mBSize = 15;
		mBFill = 0;
		mBuffer = malloc(15);
	}
	else if (mBFill == mBSize)
	{
		mBSize *= 2;
		char * b = malloc(mBSize);
		for(char i=0; i<mBFill; i++)
			b[i] = mBuffer[i];
		free(mBuffer);
		mBuffer = b;
	}
	mBuffer[mBFill++] = ch;
}

string ostringstream::str(void) const
{
	return string(mBuffer, mBFill);
}

void ostringstream::str(const string & str)
{
	mBFill = str.size();
	if (mBFill > mBSize)
	{
		free(mBuffer);
		mBSize = mBFill;
		mBuffer = malloc(mBSize);
	}
	str.copyseg(mBuffer, 0, mBFill);
}

istringstream::istringstream(const string & str)
	: mString(str), mSPos(0)
{}

istringstream::~istringstream(void)
{}

string istringstream::str(void) const
{
	return mString;	
}

void istringstream::str(const string & str)
{
	mState = goodbit;
	mString = str;
	mSPos = 0;
}


void istringstream::refill(void)
{
	mBufferFill = 0;
	mBufferPos = 0;

	char ch;
	while (mSPos < mString.size() && mBufferFill < 32)
	{
		mBuffer[mBufferFill++] = mString[mSPos++];
	}
}

}
