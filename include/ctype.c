#include "ctype.h"
#include "conio.h"

#define CC_CTRL		0x00
#define CC_BREAK	0x01
#define CC_SPACE	0x02
#define CC_DIGIT	0x04
#define CC_LOWER	0x08
#define CC_UPPER	0x10
#define CC_HEX		0x20
#define CC_PUNCT	0x40

static const char _cinfo[128] = {
	[0 ... 8] = CC_CTRL,
	[9] = CC_SPACE,
	[10 ... 13] = CC_BREAK,
	[14 ... 31] = CC_CTRL,
	[32] = CC_SPACE,
	[33 ... 47] = CC_PUNCT,
	[48 ... 57] = CC_DIGIT,
	[58 ... 64] = CC_PUNCT,
	[65 ... 70] = CC_UPPER | CC_HEX,
	[71 ... 90] = CC_UPPER,
	[91 ... 96] = CC_PUNCT,
	[97 ... 102] = CC_LOWER | CC_HEX,
	[103 ... 122] = CC_LOWER,
	[123 ... 126] = CC_PUNCT,
	[127] = CC_CTRL
};

bool isctrnl(char c)
{
	return (c < 128) && _cinfo[c] == CC_CTRL;
}

bool isprint(char c)
{
	return (c < 128) && _cinfo[c] != CC_CTRL;
}

bool isspace(char c)
{
	return (c < 128) && (_cinfo[c] & (CC_SPACE | CC_BREAK));
}

bool isblank(char c)
{
	return (c < 128) && (_cinfo[c] & CC_SPACE);
}

bool isgraph(char c)
{
	return (c < 128) && (_cinfo[c] & (CC_LOWER | CC_UPPER | CC_DIGIT | CC_PUNCT));
}

bool ispunct(char c)
{
	return (c < 128) && (_cinfo[c] & CC_PUNCT);
}

bool isalnum(char c)
{
	return (c < 128) && (_cinfo[c] & (CC_LOWER | CC_UPPER | CC_DIGIT));
}

bool isalpha(char c)
{
	return (c < 128) && (_cinfo[c] & (CC_LOWER | CC_UPPER));
}

bool isupper(char c)
{
	return (c < 128) && (_cinfo[c] & CC_UPPER);
}

bool islower(char c)
{
	return (c < 128) && (_cinfo[c] & CC_LOWER);
}

bool isdigit(char c)
{
	return (c < 128) && (_cinfo[c] & CC_DIGIT);
}

bool isxdigit(char c)
{
	return (c < 128) && (_cinfo[c] & CC_HEX);
}

char tolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	else
		return c;
}

char toupper(char c)
{
	if (c >= 'a' && c <= 'z')
		return c + ('A' - 'a');
	else
		return c;
}
