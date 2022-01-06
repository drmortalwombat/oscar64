#ifndef C64_KEYBOARD_H
#define C64_KEYBOARD_H

#include "types.h"

#define KEY_CSR_DOWN	(17)
#define KEY_CSR_RIGHT	(29)
#define KEY_CSR_UP		(17 + 128)
#define KEY_CSR_LEFT	(29 + 128)

#define KEY_ESC			(27)
#define KEY_DEL			(20)
#define KEY_INST		(148)
#define KEY_RETURN		(13)

#define KEY_F1			(133)
#define KEY_F3			(134)
#define KEY_F5			(135)
#define KEY_F7			(136)

#define KEY_F2			(137)
#define KEY_F4			(138)
#define KEY_F6			(139)
#define KEY_F8			(140)

#define KEY_CODE_CSR_RIGHT	(2)
#define KEY_CODE_CSR_DOWN	(7)
#define KEY_CODE_LSHIFT		(15)
#define KEY_CODE_RSHIFT		(48)

// map of keyboard codes to PETSCII, first 64 without shift
// second 64 with shift

extern const char keyb_codes[128];

// current status of key matrix 
extern byte keyb_matrix[8];

// current key
extern byte keyb_key;

// poll keyboard matrix

void keyb_poll(void);

inline bool key_pressed(char code);

inline bool key_shift(void);

#pragma compile("keyboard.c")

#endif
