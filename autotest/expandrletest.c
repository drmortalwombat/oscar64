#include <oscar.h>

static const char expandRleTestEncoded[] =
{
	(char)0xa2,
	(char)0x5a,
	(char)0x11,
	(char)0x22,
	(char)0x33,
	(char)0x00
};

int main(void)
{
	char output[6] = { 0 };

	oscar_expand_rle(output, expandRleTestEncoded);

	if ((unsigned char)output[0] != 0x5a ||
		(unsigned char)output[1] != 0x5a ||
		(unsigned char)output[2] != 0x5a ||
		(unsigned char)output[3] != 0x11 ||
		(unsigned char)output[4] != 0x22 ||
		(unsigned char)output[5] != 0x33)
	{
		return 1;
	}

	return 0;
}
