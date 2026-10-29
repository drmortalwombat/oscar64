#ifndef CTYPE_H
#define CTYPE_H

bool isctrnl(char c);

bool isprint(char c);

bool isspace(char c);

bool isblank(char c);

bool isgraph(char c);

bool ispunct(char c);

bool isalnum(char c);

bool isalpha(char c);

bool isupper(char c);

bool islower(char c);

bool isdigit(char c);

bool isxdigit(char c);

char tolower(char c);

char toupper(char c);

#pragma compile("ctype.c")

#endif

