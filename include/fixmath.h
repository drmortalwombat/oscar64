#ifndef FIXMATH_H
#define FIXMATH_H

// Multiply two unsinged 16bit numbers and return a 32bit result
__native unsigned long lmul16u(unsigned x, unsigned y);

// Multiply two signed 16bit numbers and return a signed 32bit result
__native long lmul16s(int x, int y);

// Multiply two 12.4 fixpoint numbers and return a 12.4 fixpoint result
inline int lmul12f4s(int x, int y);


// Multiply two 8.8 fixpoint numbers and return an 8.8 fixpoint result
inline int lmul8f8s(int x, int y);

// Multiply two 4.12 fixpoint numbers and return a 12.4 fixpoint result
__native int lmul4f12s(int x, int y);

// Square of a 4.12 sigend fixpoint number and return an 8.24 fixpoint result
inline unsigned long lsqr4f12s(int x);

// Divide a 32bit unsigned number by a 16bit number and return a 16bit number
__native unsigned ldiv16u(unsigned long x, unsigned y);

// Divide a signed 32bit number by a signed 16bit number and return a signed 16bit number
__native int ldiv16s(long x, int y);

// Divide a 12.4 fixed point number by a 12.4 fixpoint number
inline int ldiv12f4s(int x, int y);

// Divide a 8.8 fixed point number by an 8.8 fixpoint number
inline int ldiv8f8s(int x, int y);

// Divide a 4.12 fixed point number by a 4.12 fixpoint number
inline int ldiv4f12s(int x, int y);

// Multiply two unsigned 16bit numbers and divide the result by another 16bit number a * b / c
__native unsigned lmuldiv16u(unsigned a, unsigned b, unsigned c);

// Multiply two signed 16bit numbers and divide the result by another signed 16bit number a * b / c
__native int lmuldiv16s(int a, int b, int c);


__native unsigned lmuldiv16by8(unsigned a, char b, char c);

inline int lmuldiv16sby8(int a, char b, char c);

__native unsigned lmuldiv8by8(char a, char b, char c);

#pragma compile("fixmath.c")

#endif
