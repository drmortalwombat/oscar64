#include "ifstream.h"
#include <c64/kernalio.h>

namespace opp {

ifstream::ifstream(char fnum, char device, char channel, const string & name)
{
	this->fnum = fnum;
	krnio_setnam(name.tocstr());
	krnio_open(fnum, device, channel);
}

ifstream::~ifstream(void)
{
	krnio_close(fnum);
}

void ifstream::refill(void)
{
	mBufferPos = 0;
	mBufferFill = krnio_read(fnum, mBuffer, 32);
}

}
