#ifndef STDARG_H
#define STDARG_H

typedef void *va_list;

#define	va_start(list, name) (void) (list = &name + 1)

#define	va_arg(list, mode) ((mode *)(list = (char *)list + sizeof (mode)))[-1]

#define	va_end(list) (void)0

#endif
