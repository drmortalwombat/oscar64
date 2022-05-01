#ifndef OSCAR_H
#define OSCAR_H


__native const char * oscar_expand_lzo(char * dp, const char * sp);

__native const char * oscar_expand_rle(char * dp, const char * sp);


#pragma compile("oscar.c")

#endif
