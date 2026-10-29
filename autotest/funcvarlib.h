#ifndef FUNCVARLIB_H
#define FUNCVARLIB_H


struct libstruct_ab
{
	int (*f)(int x);	
};

struct libstruct_de
{
	int (*f)(int x);	
};

int libstruct_test(bool c);


typedef int (*libparam_t)(int x);

int libparam_ab(libparam_t fp, int x);

int libparam_de(libparam_t fp, int x);


#pragma compile("funcvarlib.c")

#endif