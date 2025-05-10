#ifndef STDIO_H
#define STDIO_H

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

void putchar(char c);

char getchar(void);

void puts(const char * str);

char * gets(char * str);

char * gets_s(char * str, size_t n);

void printf(const char * fmt, ...);

int sprintf(char * str, const char * fmt, ...);

void vprintf(const char * fmt, va_list vlist);

int vsprintf(char * str, const char * fmt, va_list vlist);

int scanf(const char * fmt, ...);

int sscanf(const char * str, const char * fmt, ...);

#pragma compile("stdio.c")

#endif

