#ifndef TIME_H
#define TIME_H

typedef long clock_t;

#define CLOCKS_PER_SEC	60L

clock_t clock(void);

#pragma compile("time.c")

#endif
