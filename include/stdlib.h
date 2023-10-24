#ifndef STDLIB_H
#define STDLIB_H


extern const float tpow10[7];

void itoa(int n, char * s, unsigned radix);

void utoa(unsigned int n, char * s, unsigned radix);

void ftoa(float f, char * s);

void ltoa(long n, char * s, unsigned radix);

void ultoa(unsigned long n, char * s, unsigned radix);

int atoi(const char * s);

long atol(const char * s);

float atof(const char * s);

float strtof(const char *s, const char **endp);

int strtoi(const char *s, const char **endp, char base);

unsigned strtou(const char *s, const char **endp, char base);

long strtol(const char *s, const char **endp, char base);

unsigned long strtoul(const char *s, const char **endp, char base);

int abs(int n);

long labs(long n);


void exit(int status);

void * malloc(unsigned int size);

void free(void * ptr);

void * calloc(int num, int size);

unsigned heapfree(void);

unsigned int rand(void);

void srand(unsigned int seed);

#pragma intrinsic(malloc)

#pragma intrinsic(free)

#pragma compile("stdlib.c")

#endif
