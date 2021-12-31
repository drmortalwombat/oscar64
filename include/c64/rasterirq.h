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

// One raster interrupt operation, handles up to five writes
// to arbitrary memory location, or one wait and four writes.
typedef struct RIRQCode
{	
	byte		size;
	byte		code[RIRQ_SIZE];
} RIRQCode;

// Build one raster IRQ operation of the given size (wait + #ops)
void rirq_build(RIRQCode * ic, byte size);

// Add a write command to a raster IRQ
inline void rirq_write(RIRQCode * ic, byte n, void * addr, byte data);

// Change the address of a raster IRQ write command
inline void rirq_addr(RIRQCode * ic, byte n, void * addr);

// Change the data of a raster IRQ write command
inline void rirq_data(RIRQCode * ic, byte n, byte data);

// Add a delay of 5 * cycles to a raster IRQ
inline void rirq_delay(RIRQCode * ic, byte cycles);

// Place a raster IRQ into one of the 16 slots
inline void rirq_set(byte n, byte row, RIRQCode * write);

// Remove a raster IRQ from one of the 16 slots
inline void rirq_clear(byte n)

// Change the vertical position of the raster IRQ of one of the slots
inline void rirq_move(byte n, byte row);


// Initialize the raster IRQ system with either the kernal IRQ vector
// or the hardware IRQ vector if the kernal ROM is turned off (which is
// the less resource hungry option)
void rirq_init(bool kernalIRQ);

// Start raster IRQ
void rirq_start(void);

// Stop raster IRQ
void rirq_stop(void);

// Sort the raster IRQ, must be performed at the end of the frame after changing 
// the vertical position of one of the interrupt operatins.
void rirq_sort(void);

// Wait for the last raster IRQ op to have completed.  Must be called before a
// sort if the raster IRQ system is active
void rirq_wait(void);

#pragma compile("rasterirq.c")

#endif
