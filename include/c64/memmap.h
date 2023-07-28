#ifndef MEMMAP_H
#define MEMMAP_H

#include "types.h"

#define MMAP_ROM		0x37
#define MMAP_NO_BASIC	0x36
#define MMAP_NO_ROM		0x35
#define MMAP_RAM		0x30
#define MMAP_CHAR_ROM	0x31
#define MMAP_ALL_ROM	0x33

// Install an IRQ an NMI trampoline, that routes the kernal interrupts
// through an intermediate trampoline when the kernal ROM is not paged
// in.  The trampoline enables the ROM, executes the interrupt and
// restores the memory map setting before returning.

void mmap_trampoline(void);

// Set the memory map in a way that is compatible with the IRQ
// trampoline, returns the previous state

inline char mmap_set(char pla);

#pragma compile("memmap.c")

#endif
