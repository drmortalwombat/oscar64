#ifndef STDINT_H
#define STDINT_H

typedef signed char int8_t;
typedef short int16_t;
typedef long int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;

typedef signed char int_least8_t;
typedef short int_least16_t;
typedef long int_least32_t;

typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned long uint_least32_t;

typedef signed char int_fast8_t;
typedef short int_fast16_t;
typedef long int_fast32_t;

typedef unsigned char uint_fast8_t;
typedef unsigned short uint_fast16_t;
typedef unsigned long uint_fast32_t;

typedef int intptr_t;
typedef unsigned int uintptr_t;

typedef long intmax_t;
typedef unsigned long uintmax_t;

        /* LIMIT MACROS */
#define INT8_MIN    (-0x7f - 1)
#define INT16_MIN   (-0x7fff - 1)
#define INT32_MIN   (-0x7fffffffL - 1)

#define INT8_MAX    0x7f
#define INT16_MAX   0x7fff
#define INT32_MAX   0x7fffffffL

#define UINT8_MAX   0xff
#define UINT16_MAX  0xffff
#define UINT32_MAX  0xffffffffUL

#define INT_LEAST8_MIN    (-0x7f - 1)
#define INT_LEAST16_MIN   (-0x7fff - 1)
#define INT_LEAST32_MIN   (-0x7fffffffL - 1)

#define INT_LEAST8_MAX    0x7f
#define INT_LEAST16_MAX   0x7fff
#define INT_LEAST32_MAX   0x7fffffffL

#define UINT_LEAST8_MAX   0xff
#define UINT_LEAST16_MAX  0xffff
#define UINT_LEAST32_MAX  0xffffffffUL

#define INT_FAST8_MIN     (-0x7f - 1)
#define INT_FAST16_MIN    (-0x7fff - 1)
#define INT_FAST32_MIN    (-0x7fffffffL - 1)

#define INT_FAST8_MAX     0x7f
#define INT_FAST16_MAX    0x7fff
#define INT_FAST32_MAX    0x7fffffffL

#define UINT_FAST8_MAX    0xff
#define UINT_FAST16_MAX   0xffff
#define UINT_FAST32_MAX   0xffffffffUL

#define INTPTR_MIN        (-0x7fff - 1)
#define INTPTR_MAX        0x7fff
#define UINTPTR_MAX       0xffff

#define INT8_C(x)    (x)
#define INT16_C(x)   (x)
#define INT32_C(x)   ((x) + (INT32_MAX - INT32_MAX))

#define UINT8_C(x)   (x)
#define UINT16_C(x)  (x)
#define UINT32_C(x)  ((x) + (UINT32_MAX - UINT32_MAX))

#define INTMAX_C(x)  ((x) + (INT32_MAX - INT32_MAX))
#define UINTMAX_C(x) ((x) + (UINT32_MAX - UINT32_MAX))

#define PTRDIFF_MIN  INT16_MIN
#define PTRDIFF_MAX  INT16_MAX

#define SIZE_MAX     UINT16_MAX

#define INTMAX_MIN        (-0x7fffffffL - 1)
#define INTMAX_MAX        0x7fffffffL
#define UINTMAX_MAX       0xffffffffUL


#endif
