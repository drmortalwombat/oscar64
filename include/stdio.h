#ifndef STDIO_H
#define STDIO_H

#include <stdlib.h>

void putchar(char c);

char getchar(void);

void puts(const char * str);

char * gets(char * str);

void printf(const char * fmt, ...);

int sprintf(char * str, const char * fmt, ...);

#pragma compile("stdio.c")

#endif

