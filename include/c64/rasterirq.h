#ifndef C64_RASTERIRQ_H
#define C64_RASTERIRQ_H

#include "types.h"

#ifndef NUM_IRQS
#define NUM_IRQS		16
#endif

extern volatile byte rirq_count;

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

	RIRQ_SIZE   = 31,

	RIRQ_DATA_5 = 31,
	RIRQ_ADDR_5 = 33,

	RIRQ_DATA_6 = 36,
	RIRQ_ADDR_6 = 38,

	RIRQ_DATA_7 = 41,
	RIRQ_ADDR_7 = 43,

	RIRQ_DATA_8 = 46,
	RIRQ_ADDR_8 = 48,

	RIRQ_DATA_9 = 51,
	RIRQ_ADDR_9 = 53,

	RIRQ_DATA_10 = 56,
	RIRQ_ADDR_10 = 58,

	RIRQ_DATA_11 = 61,
	RIRQ_ADDR_11 = 63,

	RIRQ_DATA_12 = 66,
	RIRQ_ADDR_12 = 68,

	RIRQ_DATA_13 = 71,
	RIRQ_ADDR_13 = 73,

	RIRQ_DATA_14 = 76,
	RIRQ_ADDR_14 = 78,

	RIRQ_DATA_15 = 81,
	RIRQ_ADDR_15 = 83,

	RIRQ_DATA_16 = 86,
	RIRQ_ADDR_16 = 88,

	RIRQ_DATA_17 = 91,
	RIRQ_ADDR_17 = 93,

	RIRQ_DATA_18 = 96,
	RIRQ_ADDR_18 = 98,

	RIRQ_DATA_19 = 101,
	RIRQ_ADDR_19 = 103,

	RIRQ_SIZE_20 = 106,

	RIRQ_DATA_20 = 106,
	RIRQ_ADDR_20 = 108,

	RIRQ_DATA_21 = 111,
	RIRQ_ADDR_21 = 113,

	RIRQ_DATA_22 = 116,
	RIRQ_ADDR_22 = 118,

	RIRQ_DATA_23 = 121,
	RIRQ_ADDR_23 = 123,

	RIRQ_DATA_24 = 126,
	RIRQ_ADDR_24 = 128,

	RIRQ_DATA_25 = 131,
	RIRQ_ADDR_25 = 133
};

// One raster interrupt operation, handles up to five writes
// to arbitrary memory location, or one wait and four writes.
typedef struct RIRQCode
{	
	byte		size;
	byte		code[RIRQ_SIZE];
} RIRQCode;

typedef struct RQIRCode20
{
	RIRQCode	c;
	byte		code[RIRQ_SIZE_20 - RIRQ_SIZE];	
} RIRQCode20;

// Build one raster IRQ operation of the given size (wait + #ops) for up to 5 instructions
void rirq_build(RIRQCode * ic, byte size);

// Allocate one raster IRQ operation of the given size (wait + #ops) 
RIRQCode * rirq_alloc(byte size);

// Add a write command to a raster IRQ
inline void rirq_write(RIRQCode * ic, byte n, void * addr, byte data);

// Add a call command to a raster IRQ
inline void rirq_call(RIRQCode * ic, byte n, void * addr);

// Change the address of a raster IRQ write command
inline void rirq_addr(RIRQCode * ic, byte n, void * addr);

// Change the high byte of the address of a raster IRQ write command
inline void rirq_addrhi(RIRQCode * ic, byte n, byte hi);

// Change the data of a raster IRQ write command
inline void rirq_data(RIRQCode * ic, byte n, byte data);

// Add a delay of 5 * cycles to a raster IRQ
inline void rirq_delay(RIRQCode * ic, byte cycles);

// Place a raster IRQ into one of the 16 slots, the interrupt will fire
// one line below the given row
inline void rirq_set(byte n, byte row, RIRQCode * write);

// Remove a raster IRQ from one of the 16 slots
inline void rirq_clear(byte n);

// Change the vertical position of the raster IRQ of one of the slots
inline void rirq_move(byte n, byte row);


// Initialize the raster IRQ system with either the kernal IRQ vector
// or the hardware IRQ vector if the kernal ROM is turned off (which is
// the less resource hungry option)
inline void rirq_init(bool kernalIRQ);

// Raster IRQ through kernal, with IO range always enabled
// calls kernal continuation
void rirq_init_kernal(void);

// Raster IRQ through kernal, with IO range not always enabled
// calls kernal continuation
void rirq_init_kernal_io(void);

// Raster IRQ through RAM and ROM vector, with ROM disabled or not and IO range always enabled
// does not call kernal continuation
void rirq_init_crt(void);

// Raster IRQ through RAM and ROM vector, with ROM disabled or not and IO range not always enabled
// does not call kernal continuation
void rirq_init_crt_io(void);

// Raster IRQ through RAM vector, with ROM disabled and IO range always enabled
// does not call kernal continuation
void rirq_init_io(void);

// Raster IRQ through RAM vector, with ROM disabled and IO range not always enabled
// does not call kernal continuation
void rirq_init_memmap(void);

// Start raster IRQ
void rirq_start(void);

// Stop raster IRQ
void rirq_stop(void);

// Sort the raster IRQ, must be performed at the end of the frame after changing 
// the vertical position of one of the interrupt operatins.
// Set the inirq flag to true when calling this from an interrupt
void rirq_sort(bool inirq = false);

// Wait for the last raster IRQ op to have completed.  Must be called before a
// sort if the raster IRQ system is active
void rirq_wait(void);

#pragma compile("rasterirq.c")

#endif
