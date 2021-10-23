#ifndef STDDEF_H
#define STDDEF_H

#define NULL nullptr

typedef unsigned int	size_t;
typedef signed int		ptrdiff_t;

#define offsetof(st, m)	((size_t)&(((st *)0)->m))

#endif
