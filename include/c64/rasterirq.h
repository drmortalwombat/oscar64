#ifndef C64_RASTERIRQ_H
#define C64_RASTERIRQ_H

#include "types.h"

#define NUM_IRQS		16


enum RIRQCodeIndex
{
	RIRQ_DATA_0	=  1,
	RIRQ_DATA_1 =  3,

	RIRQ_ADDR_0 = 10,
	RIRQ_ADDR_1 = 13,

	RIRQ_DATA_2 = 16,
	RIRQ_ADDR_2 = 18,

	RIRQ_DATA_3 = 21,
	RIRQ_ADDR_3 = 23,

	RIRQ_DATA_4 = 26,
	RIRQ_ADDR_4 = 28,

	RIRQ_SIZE   = 31
};

typedef struct RIRQCode
{	
	byte		size;
	byte		code[RIRQ_SIZE];
} RIRQCode;

void rirq_build(RIRQCode * ic, byte size);

inline void rirq_write(RIRQCode * ic, byte n, void * addr, byte data);
inline void rirq_addr(RIRQCode * ic, byte n, void * addr);
inline void rirq_data(RIRQCode * ic, byte n, byte data);

inline void rirq_set(byte n, byte row, RIRQCode * write);
inline void rirq_clear(byte n)
inline void rirq_move(byte n, byte row);


void rirq_init(bool kernalIRQ);
void rirq_start(void);
void rirq_stop(void);
void rirq_sort(void);
void rirq_wait(void);

#pragma compile("rasterirq.c")

#endif
