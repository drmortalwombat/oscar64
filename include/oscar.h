#ifndef OSCAR_H
#define OSCAR_H


__native const char * oscar_expand_lzo(char * dp, const char * sp);

__native const char * oscar_expand_rle(char * dp, const char * sp);

// Same as oscar_expand_lzo, but does not need read access to the
// target memory area.  On the downside, it uses 256 bytes of stack
// memory as buffer
__native const char * oscar_expand_lzo_buf(char * dp, const char * sp);

#pragma compile("oscar.c")

#endif
