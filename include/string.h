#ifndef STRING_H
#define STRING_H

char * strcpy(char * dst, const char * src);

int strcmp(const char * ptr1, const char * ptr2);

int strlen(const char * str);

char * strcat(char * dst, const char * src);

void * memclr(void * dst, int size);

void * memset(void * dst, int value, int size);

void * memcpy(void * dst, const void * src, int size);

int memcmp(const void * ptr1, const void * ptr2, int size);

void * memmove(void * dst, const void * src, int size);

#pragma compile("string.c")

#endif

