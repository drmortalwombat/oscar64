#include "ctype.h"
#include "conio.h"

static inline bool isin(char a, char l, char h)
{
	return (char)(a - l) <= (char)(h - l);
}

bool isctrnl(char c)
{
	return c == 0x7f || isin(c, 0x00, 0x1f);
}

bool isprint(char c)
{
	return isin(c, 0x20, 0x7e);
}

bool isspace(char c)
{
	return c == 0x20 || isin(c, 0x09, 0x0d);
}

bool isblank(char c)
{
	return c == 0x20 || c == 0x09;
}

bool isgraph(char c)
{
	return isin(c, 0x21, 0x7e);
}

bool ispunct(char c)
{
	return isin(c, 0x21, 0x2f) || isin(c, 0x3a, 0x40) || isin(c, 0x5b, 0x60) || isin(c, 0x7b, 0x7e);
}

bool isalnum(char c)
{
	return isin(c, '0', '9') || isin(c, 'A', 'Z') || isin(c, 'a', 'z');
}

bool isalpha(char c)
{
	return isin(c, 'A', 'Z') || isin(c, 'a', 'z');
}

bool isupper(char c)
{
	return isin(c, 'A', 'Z');
}

bool islower(char c)
{
	return isin(c, 'a', 'z');
}

bool isdigit(char c)
{
	return isin(c, '0', '9');
}

bool isxdigit(char c)
{
	return isin(c, '0', '9') || isin(c, 'A', 'F') || isin(c, 'a', 'f');
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
