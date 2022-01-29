#ifndef FIXMATH_H
#define FIXMATH_H

__native unsigned long lmul16u(unsigned x, unsigned y);

__native long lmul16s(int x, int y)

inline int lmul12f4s(int x, int y);

inline int lmul8f8s(int x, int y);

__native int lmul4f12s(int x, int y)

__native unsigned ldiv16u(unsigned long x, unsigned y)

__native int ldiv16s(long x, int y)

inline int ldiv12f4s(int x, int y)

inline int ldiv8f8s(int x, int y)

inline int ldiv4f12s(int x, int y)

__native unsigned lmuldiv16u(unsigned a, unsigned b, unsigned c)

__native int lmuldiv16s(int a, int b, int c)

#pragma compile("fixmath.c")

#endif
