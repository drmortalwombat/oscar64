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


#define FOPEN_MAX		8
#define FILENAME_MAX	16
#define EOF				-1

struct FILE;

extern FILE * stdin;
extern FILE * stdout;

FILE * fopen(const char * fname, const char * mode);

int fclose(FILE * fp);

int fgetc(FILE* stream);

char* fgets(char* s, int n, FILE* stream);

int fputc(int c, FILE* stream);

int fputs(const char* s, FILE* stream);

int feof(FILE * stream);

size_t fread( void * buffer, size_t size, size_t count, FILE * stream );

size_t fwrite( const void* buffer, size_t size, size_t count, FILE* stream );

int fprintf( FILE * stream, const char* format, ... );

int fscanf( FILE *stream, const char *format, ... );

#pragma compile("stdio.c")

#endif

