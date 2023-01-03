#ifndef C64_KEYBOARD_H
#define C64_KEYBOARD_H

#include "types.h"

#define KEY_CSR_DOWN	(17)
#define KEY_CSR_RIGHT	(29)
#define KEY_CSR_UP		(17 + 128)
#define KEY_CSR_LEFT	(29 + 128)

#define KEY_ARROW_LEFT	(95)

#define KEY_ESC			(27)
#define KEY_DEL			(20)
#define KEY_INST		(148)
#define KEY_RETURN		(13)

#define KEY_HOME		(19)
#define KEY_CLR			(147)

#define KEY_F1			(133)
#define KEY_F3			(134)
#define KEY_F5			(135)
#define KEY_F7			(136)

#define KEY_F2			(137)
#define KEY_F4			(138)
#define KEY_F6			(139)
#define KEY_F8			(140)

enum KeyScanCode
{
	KSCAN_DEL,
	KSCAN_RETURN,
	KSCAN_CSR_RIGHT,
	KSCAN_F7,
	KSCAN_F1,
	KSCAN_F3,
	KSCAN_F5,
	KSCAN_CSR_DOWN,

	KSCAN_3,
	KSCAN_W,
	KSCAN_A,
	KSCAN_4,
	KSCAN_Z,
	KSCAN_S,
	KSCAN_E,
	KSCAN_SHIFT_LOCK,

	KSCAN_5,
	KSCAN_R,
	KSCAN_D,
	KSCAN_6,
	KSCAN_C,
	KSCAN_F,
	KSCAN_T,
	KSCAN_X,

	KSCAN_7,
	KSCAN_Y,
	KSCAN_G,
	KSCAN_8,
	KSCAN_B,
	KSCAN_H,
	KSCAN_U,
	KSCAN_V,

	KSCAN_9,
	KSCAN_I,
	KSCAN_J,
	KSCAN_0,
	KSCAN_M,
	KSCAN_K,
	KSCAN_O,
	KSCAN_N,

	KSCAN_PLUS,
	KSCAN_P,
	KSCAN_L,
	KSCAN_MINUS,
	KSCAN_DOT,
	KSCAN_COLON,
	KSCAN_AT,
	KSCAN_COMMA,

	KSCAN_POUND,
	KSCAN_STAR,
	KSCAN_SEMICOLON,
	KSCAN_HOME,
	KSCAN_RSHIFT,
	KSCAN_EQUAL,
	KSCAN_ARROW_UP,
	KSCAN_SLASH,

	KSCAN_1,
	KSCAN_ARROW_LEFT,
	KSCAN_CONTROL,
	KSCAN_2,
	KSCAN_SPACE,
	KSCAN_COMMODORE,
	KSCAN_Q,
	KSCAN_STOP,

	KSCAN_QUAL_SHIFT = 0x40,
	KSCAN_QUAL_MASK	= 0x7f,
	KSCAN_QUAL_DOWN = 0x80,

	KSCAN_MAX = 0xff
};

// map of keyboard codes to PETSCII, first 64 without shift
// second 64 with shift

extern const char keyb_codes[128];

// current status of key matrix 
extern byte keyb_matrix[8];

// current key in scan code - the top level bit KSCAN_QUAL_DOWN is
// used to indicate a key is pressed, so 0 is no key
extern KeyScanCode keyb_key;

// poll keyboard matrix

void keyb_poll(void);

inline bool key_pressed(KeyScanCode code);

inline bool key_shift(void);

#pragma compile("keyboard.c")

#endif
