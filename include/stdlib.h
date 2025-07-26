#ifndef STDLIB_H
#define STDLIB_H

#define RAND_MAX	65535u

typedef struct
{
	int quot;
	int rem;
} div_t;

typedef struct
{
	long int quot;
	long int rem;
} ldiv_t;

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

div_t div(int numer, int denom);

ldiv_t ldiv(long int numer, long int denom);

void exit(int status);

void abort(void);

void * malloc(unsigned int size);

void free(void * ptr);

void * calloc(int num, int size);

void * realloc(void * ptr, unsigned size);

unsigned heapfree(void);

unsigned int rand(void);

void srand(unsigned int seed);

unsigned long lrand(void);

void lsrand(unsigned long seed);

#pragma intrinsic(malloc)

#pragma intrinsic(free)

#pragma compile("stdlib.c")

#endif
