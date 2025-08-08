#ifndef STRING_H
#define STRING_H

char * strcpy(char * dst, const char * src);

char * strncpy(char * dst, const char * src, int n);

signed char strcmp(const char * ptr1, const char * ptr2);

signed char strncmp(const char * ptr1, const char * ptr2, int n);

int strlen(const char * str);

char * strcat(char * dst, const char * src);

char * strncat(char * dst, const char * src, int n);

char * strchr(const char * str, int ch);

char * strrchr(const char * str, int ch);

char * strstr(const char * str, const char * substr);

char * cpycat(char * dst, const char * src);

void * memclr(void * dst, int size);

void * memset(void * dst, int value, int size);

void * memcpy(void * dst, const void * src, int size);

signed char memcmp(const void * ptr1, const void * ptr2, int size);

void * memmove(void * dst, const void * src, int size);

void * memchr(const void * ptr, int ch, int size);

#pragma intrinsic(strcpy)

#pragma intrinsic(memcpy)

#pragma intrinsic(memset)

#pragma intrinsic(memclr)

#pragma compile("string.c")

#endif
