#include "funcvarlib.h"

int libstruct_a(int x)
{
	return 1 * x;
}

int libstruct_b(int x)
{
	return 2 * x;
}

int libstruct_d(int x)
{
	return x;
}

__noinline int libstruct_k(int x, int t)
{
	return t * x;
}

int libstruct_e(int x)
{
	return libstruct_k(4, x);
}

__noinline int libstruct_u(int x)
{
	return libstruct_k(4, x);
}


__noinline int libstruct_test(bool c)
{
	libstruct_ab	ab;
	libstruct_de	de;

	if (c)
	{
		ab.f = libstruct_a;
		de.f = libstruct_d;
	}
	else
	{
		ab.f = libstruct_b;
		de.f = libstruct_e;
	}

	return ab.f(1) + de.f(2) + libstruct_u(3);
}





__noinline int libparam_ab(libparam_t fp, int x)
{
	return fp(x);
}

__noinline int libparam_de(libparam_t fp, int x)
{
	return fp(x);
}

