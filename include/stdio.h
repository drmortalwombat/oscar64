#ifndef STDIO_H
#define STDIO_H

#include <stdlib.h>


void putchar(char c);

char getchar(void);

void puts(const char * str);

char * gets(char * str);

void printf(const char * fmt, ...);

int sprintf(char * str, const char * fmt, ...);

int scanf(const char * fmt, ...);

int sscanf(const char * fmt, const char * str, ...);

#pragma compile("stdio.c")

#endif

