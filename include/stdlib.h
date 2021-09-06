#ifndef STDLIB_H
#define STDLIB_H

void itoa(int n, char * s, int radix);

void utoa(unsigned int n, char * s, unsigned int radix);

void ftoa(float f, char * s);

int atoi(const char * s);

void exit(int status);

void * malloc(unsigned int size);

void free(void * ptr);

void * calloc(int num, int size);

unsigned int rand(void);

#pragma compile("stdlib.c")

#endif
