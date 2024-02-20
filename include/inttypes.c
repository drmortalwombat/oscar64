#include "inttypes.h"
#include "stdlib.h"


intmax_t  imaxabs(intmax_t n)
{
	return n < 0 ? -n : n;
}

imaxdiv_t imaxdiv(intmax_t l, intmax_t r)
{
	imaxdiv_t	t;
	t.quot = l / r;
	t.rem = l % r;
	return t;
}

inline intmax_t strtoimax(const char * s, char ** endp, int base)
{
	return strtol(s, endp, base);
}

inline uintmax_t strtoumax(const char * s, char ** endp, int base)
{
	return strtoul(s, endp, base);
}
