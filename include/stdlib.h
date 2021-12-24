#ifndef STDLIB_H
#define STDLIB_H


extern const float tpow10[7];

void itoa(int n, char * s, unsigned radix);

void utoa(unsigned int n, char * s, unsigned radix);

void ftoa(float f, char * s);

void ltoa(long n, char * s, unsigned radix);

void ultoa(unsigned long n, char * s, unsigned radix);

int atoi(const char * s);

float atof(const char * s);


void exit(int status);

void * malloc(unsigned int size);

void free(void * ptr);

void * calloc(int num, int size);

unsigned int rand(void);

#pragma compile("stdlib.c")

#endif
