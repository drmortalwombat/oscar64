#include <assert.h>
#include "funcvarlib.h"

int cond_a(int x)
{
	return 1 * x;
}

int cond_b(int x)
{
	return 2 * x;
}

int cond_d(int x)
{
	return x;
}

__noinline int cond_k(int x, int t)
{
	return t * x;
}

int cond_e(int x)
{
	return cond_k(4, x);
}

__noinline int cond_u(int x)
{
	return cond_k(4, x);
}

__noinline int cond_test(bool c)
{
	return (c ? cond_a : cond_b)(1) + (c ? cond_d : cond_e)(2) + cond_u(3);
}



int gvar_a(int x)
{
	return 1 * x;
}

int gvar_b(int x)
{
	return 2 * x;
}

int gvar_d(int x)
{
	return x;
}

__noinline int gvar_k(int x, int t)
{
	return t * x;
}

int gvar_e(int x)
{
	return gvar_k(4, x);
}

__noinline int gvar_u(int x)
{
	return gvar_k(4, x);
}

int (*gvar_ab)(int x);
int (*gvar_de)(int x);

__noinline int gvar_test(bool c)
{
	if (c)
	{
		gvar_ab = gvar_a;
		gvar_de = gvar_d;
	}
	else
	{
		gvar_ab = gvar_b;
		gvar_de = gvar_e;
	}

	return gvar_ab(1) + gvar_de(2) + gvar_u(3);
}



int lvar_a(int x)
{
	return 1 * x;
}

int lvar_b(int x)
{
	return 2 * x;
}

int lvar_d(int x)
{
	return x;
}

__noinline int lvar_k(int x, int t)
{
	return t * x;
}

int lvar_e(int x)
{
	return lvar_k(4, x);
}

__noinline int lvar_u(int x)
{
	return lvar_k(4, x);
}

__noinline int lvar_test(bool c)
{
	int (*lvar_ab)(int x);
	int (*lvar_de)(int x);

	if (c)
	{
		lvar_ab = lvar_a;
		lvar_de = lvar_d;
	}
	else
	{
		lvar_ab = lvar_b;
		lvar_de = lvar_e;
	}

	return lvar_ab(1) + lvar_de(2) + lvar_u(3);
}



int param_a(int x)
{
	return 1 * x;
}

int param_b(int x)
{
	return 2 * x;
}

int param_d(int x)
{
	return x;
}

__noinline int param_k(int x, int t)
{
	return t * x;
}

int param_e(int x)
{
	return param_k(4, x);
}

__noinline int param_u(int x)
{
	return param_k(4, x);
}

typedef int (*param_t)(int x);

__noinline int param_ab(param_t fp, int x)
{
	return fp(x);
}

__noinline int param_de(param_t fp, int x)
{
	return fp(x);
}

__noinline int param_test(bool c)
{
	int k;
	if (c)
	{
		k = param_ab(param_a, 1) + param_de(param_d, 2);
	}
	else
	{
		k = param_ab(param_b, 1) + param_de(param_e, 2);
	}

	return k + param_u(3);
}




int struct_a(int x)
{
	return 1 * x;
}

int struct_b(int x)
{
	return 2 * x;
}

int struct_d(int x)
{
	return x;
}

__noinline int struct_k(int x, int t)
{
	return t * x;
}

int struct_e(int x)
{
	return struct_k(4, x);
}

__noinline int struct_u(int x)
{
	return struct_k(4, x);
}

struct struct_ab
{
	int (*f)(int x);	
};

struct struct_de
{
	int (*f)(int x);	
};

__noinline int struct_test(bool c)
{
	struct_ab	ab;
	struct_de	de;

	if (c)
	{
		ab.f = struct_a;
		de.f = struct_d;
	}
	else
	{
		ab.f = struct_b;
		de.f = struct_e;
	}

	return ab.f(1) + de.f(2) + struct_u(3);
}




int pstruct_a(int x)
{
	return 1 * x;
}

int pstruct_b(int x)
{
	return 2 * x;
}

int pstruct_d(int x)
{
	return x;
}

__noinline int pstruct_k(int x, int t)
{
	return t * x;
}

int pstruct_e(int x)
{
	return pstruct_k(4, x);
}

__noinline int pstruct_u(int x)
{
	return pstruct_k(4, x);
}

typedef int (*pstruct_ft)(int x);	

struct pstruct_ab
{
	int (*f)(int x);	
};

struct pstruct_de
{
	int (*f)(int x);	
};


__noinline void pstruct_a_set(pstruct_ab * pab, pstruct_ft f)
{
	pab->f = f;
}

__noinline void pstruct_b_set(pstruct_ab * pab, pstruct_ft f)
{
	pab->f = f;
}

__noinline void pstruct_d_set(pstruct_de * pde, pstruct_ft f)
{
	pde->f = f;
}

__noinline void pstruct_e_set(pstruct_de * pde, pstruct_ft f)
{
	pde->f = f;
}

__noinline int pstruct_test(bool c)
{
	pstruct_ab	ab;
	pstruct_de	de;

	if (c)
	{
		pstruct_a_set(&ab, pstruct_a);
		pstruct_d_set(&de, pstruct_d);
	}
	else
	{
		pstruct_b_set(&ab, pstruct_b);
		pstruct_e_set(&de, pstruct_e);
	}

	return ab.f(1) + de.f(2) + pstruct_u(3);
}


int cast_a(int x)
{
	return 1 * x;
}

int cast_b(int x)
{
	return 2 * x;
}

typedef int (*cast_t)(int x);

__noinline int cast_test(bool b)
{
	volatile cast_t	f;

	if (b)
		f = cast_t(cast_a);
	else
		f = cast_t(cast_b);

	return cast_t(f)(11);
}


int recurse_a(int x)
{
	if (x > 0) return recurse_a(x - 1) + 2;
	else return 0;
}

int recurse_b(int x)
{
	return x * 3;
}

int recurse_d(int x)
{
	return x * 4;
}

int recurse_e(int x)
{
	return x * 5;
}

__noinline int recurse_test(bool b)
{
	return (b ? recurse_a : recurse_b)(5) + (b ? recurse_d : recurse_e)(3);
}


int void_a(int x)
{
	return 1 * x;
}

int void_b(int x)
{
	return 2 * x;
}

int void_d(int x)
{
	return x;
}

__noinline int void_k(int x, int t)
{
	return t * x;
}

int void_e(int x)
{
	return void_k(4, x);
}

__noinline int void_u(int x)
{
	return void_k(4, x);
}

typedef int (*void_t)(int x);

__noinline int void_test(bool c)
{
	void *	ab;
	void *	de;

	if (c)
	{
		ab = void_a;
		de = void_d;
	}
	else
	{
		ab = void_b;
		de = void_e;
	}

	return void_t(ab)(1) + void_t(de)(2) + void_u(3);
}




int freturn_a(int x)
{
	return 1 * x;
}

int freturn_b(int x)
{
	return 2 * x;
}

int freturn_d(int x)
{
	return x;
}

__noinline int freturn_k(int x, int t)
{
	return t * x;
}

int freturn_e(int x)
{
	return freturn_k(4, x);
}

__noinline int freturn_u(int x)
{
	return freturn_k(4, x);
}

typedef int (*freturn_t)(int x);

__noinline freturn_t freturn_ab(bool c)
{
	if (c)
		return freturn_a;
	else
		return freturn_b;
}

__noinline freturn_t freturn_de(bool c)
{
	if (c)
		return freturn_d;
	else
		return freturn_e;
}


__noinline int freturn_test(bool c)
{
	return freturn_ab(c)(1) + freturn_de(c)(2) + freturn_u(3);
}


int fargreturn_a(int x)
{
	return 1 * x;
}

int fargreturn_b(int x)
{
	return 2 * x;
}

int fargreturn_d(int x)
{
	return x;
}

__noinline int fargreturn_k(int x, int t)
{
	return t * x;
}

int fargreturn_e(int x)
{
	return fargreturn_k(4, x);
}

__noinline int fargreturn_u(int x)
{
	return fargreturn_k(4, x);
}

typedef int (*fargreturn_t)(int x);

__noinline fargreturn_t fargreturn_ab(bool c, fargreturn_t a, fargreturn_t b)
{
	if (c)
		return a;
	else
		return b;
}

__noinline fargreturn_t fargreturn_de(bool c, fargreturn_t d, fargreturn_t e)
{
	if (c)
		return d;
	else
		return e;
}


__noinline int fargreturn_test(bool c)
{
	return fargreturn_ab(c, fargreturn_a, fargreturn_b)(1) + fargreturn_de(c, fargreturn_d, fargreturn_e)(2) + fargreturn_u(3);
}



int libparam_a(int x)
{
	return 1 * x;
}

int libparam_b(int x)
{
	return 2 * x;
}

int libparam_d(int x)
{
	return x;
}

__noinline int libparam_k(int x, int t)
{
	return t * x;
}

int libparam_e(int x)
{
	return libparam_k(4, x);
}

__noinline int libparam_u(int x)
{
	return libparam_k(4, x);
}

__noinline int libparam_test(bool c)
{
	int k;
	if (c)
	{
		k = libparam_ab(param_a, 1) + libparam_de(param_d, 2);
	}
	else
	{
		k = libparam_ab(param_b, 1) + libparam_de(param_e, 2);
	}

	return k + libparam_u(3);
}


struct castruct_ab
{
	int  	a;
	int (* f)(int x);
};

struct castruct_de
{
	int a;
	int (* f)(int x);
};


int castruct_a(int x)
{
	return 1 * x;
}

int castruct_b(int x)
{
	return 2 * x;
}

int castruct_d(int x)
{
	return x;
}

__noinline int castruct_k(int x, int t)
{
	return t * x;
}

int castruct_e(int x)
{
	return castruct_k(4, x);
}

__noinline int castruct_u(int x)
{
	return castruct_k(4, x);
}

castruct_ab		castruct_const_ab[] = {
	{0, castruct_b},
	{1, castruct_a}
};

castruct_de		castruct_const_de[] = {
	{0, castruct_e},
	{1, castruct_d}
};

__noinline int castruct_test(bool c)
{
	return castruct_const_ab[c].f(1) + castruct_const_de[c].f(2) + freturn_u(3);
}


int carray_a(int x)
{
	return 1 * x;
}

int carray_b(int x)
{
	return 2 * x;
}

int carray_d(int x)
{
	return x;
}

__noinline int carray_k(int x, int t)
{
	return t * x;
}

int carray_e(int x)
{
	return carray_k(4, x);
}

__noinline int carray_u(int x)
{
	return carray_k(4, x);
}


typedef int (*carray_fab)(int x);
typedef int (*carray_fde)(int x);

carray_fab		carray_const_ab[] = {
	carray_b,
	carray_a
};

carray_fde		carray_const_de[] = {
	carray_e,
	carray_d
};

__noinline int carray_test(bool c)
{
	return carray_const_ab[c](1) + carray_const_de[c](2) + freturn_u(3);
}


int main(void)
{
	assert(cond_test(false) == 22);
	assert(cond_test(true) == 15);

	assert(gvar_test(false) == 22);
	assert(gvar_test(true) == 15);

	assert(param_test(false) == 22);
	assert(param_test(true) == 15);

	assert(lvar_test(false) == 22);
	assert(lvar_test(true) == 15);

	assert(struct_test(false) == 22);
	assert(struct_test(true) == 15);

	assert(pstruct_test(false) == 22);
	assert(pstruct_test(true) == 15);

	assert(cast_test(true) == 11);
	assert(cast_test(false) == 22);

	assert(recurse_test(true) == 10 + 12);
	assert(recurse_test(false) == 15 + 15);

	assert(void_test(false) == 22);
	assert(void_test(true) == 15);

	assert(freturn_test(false) == 22);
	assert(freturn_test(true) == 15);

	assert(fargreturn_test(false) == 22);
	assert(fargreturn_test(true) == 15);

	assert(libstruct_test(false) == 22);
	assert(libstruct_test(true) == 15);

	assert(libparam_test(false) == 22);
	assert(libparam_test(true) == 15);

	assert(castruct_test(false) == 22);
	assert(castruct_test(true) == 15);

	assert(carray_test(false) == 22);
	assert(carray_test(true) == 15);
}
