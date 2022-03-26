#ifndef CTYPE_H
#define CTYPE_H

inline bool isctrnl(char c);

inline bool isprint(char c);

inline bool isspace(char c);

inline bool isblank(char c);

inline bool isgraph(char c);

inline bool ispunct(char c);

inline bool isalnum(char c);

inline bool isalpha(char c);

inline bool isupper(char c);

inline bool islower(char c);

inline bool isdigit(char c);

inline bool isxdigit(char c);


#pragma compile("ctype.c")

#endif

