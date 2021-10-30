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

extern const char keyb_codes[128];

extern byte keyb_matrix[8], keyb_key;

void keyb_poll(void);

#pragma compile("keyboard.c")

#endif
