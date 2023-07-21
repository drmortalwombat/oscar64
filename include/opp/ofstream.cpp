#include "ofstream.h"
#include <c64/kernalio.h>

ofstream::ofstream(char fnum, char device, char channel, const string & name)
{
	this->fnum = fnum;
	krnio_setnam(name.tocstr());
	krnio_open(fnum, device, channel);

	mBufferFill = 0;
}

ofstream::~ofstream(void)
{
	if (mBufferFill > 0)
		krnio_write(fnum, mBuffer, mBufferFill);
	krnio_close(fnum);
}

void ofstream::bput(char ch)
{
	mBuffer[mBufferFill++] = ch;
	if (mBufferFill == 32)
	{
		krnio_write(fnum, mBuffer, mBufferFill);
		mBufferFill = 0;
	}
}

