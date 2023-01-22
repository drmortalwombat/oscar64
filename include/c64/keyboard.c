#include "keyboard.h"
#include "cia.h"

const char keyb_codes[128] = {
	KEY_DEL, KEY_RETURN, KEY_CSR_RIGHT, KEY_F7, KEY_F1, KEY_F3, KEY_F5, KEY_CSR_DOWN,
	'3', 'w', 'a', '4', 'z', 's', 'e',   0,
	'5', 'r', 'd', '6', 'c', 'f', 't', 'x',
	'7', 'y', 'g', '8', 'b', 'h', 'u', 'v',
	'9', 'i', 'j', '0', 'm', 'k', 'o', 'n',
	'+', 'p', 'l', '-', '.', ':', '@', ',',
	0  , '*', ';', KEY_HOME,   0, '=', '^', '/',
	'1', KEY_ARROW_LEFT, 0, '2', ' ',   0, 'q', KEY_ESC,

	KEY_INST, KEY_RETURN, KEY_CSR_LEFT, KEY_F8, KEY_F2, KEY_F4, KEY_F6, KEY_CSR_UP,
	'#', 'W', 'A', '$', 'Z', 'S', 'E',   0,
	'%', 'R', 'D', '&', 'C', 'F', 'T', 'X',
	'\'', 'Y', 'G', '(', 'B', 'H', 'U', 'V',
	')', 'I', 'J', '0', 'M', 'K', 'O', 'N',
	  0, 'P', 'L',   0, '>', '[', '@', '<',
	  0,   0, ']', KEY_CLR,   0,   0, '^', '?',
	'!',   0,   0, '"', ' ',   0, 'Q', KEY_ESC,

};


byte keyb_matrix[8];

KeyScanCode		keyb_key;
static byte keyb_pmatrix[8];

bool key_pressed(KeyScanCode code)
{
	return !(keyb_matrix[code >> 3] & (1 << (code & 7)));
}

bool key_shift(void)
{
	return 
		!(keyb_matrix[6] & 0x10) ||
		!(keyb_matrix[1] & 0x80);
}

void keyb_poll(void)
{
	cia1.ddra = 0xff;
    cia1.pra = 0xff;
	keyb_key = 0x00;

	if (cia1.prb == 0xff)
	{
		cia1.ddrb = 0x00;
		cia1.pra = 0x00;

		if (cia1.prb != 0xff)
		{
			keyb_matrix[6] &= 0xef;
			keyb_matrix[1] &= 0x7f;

			byte a = 0xfe;
			for(byte i=0; i<8; i++)
			{
				cia1.pra = a;
				a = (a << 1) | 1;

				byte p = keyb_matrix[i];
				byte k = cia1.prb;
				keyb_matrix[i] = k;

				k = (k ^ 0xff) & p;
				if (k)
				{
					byte j = 8 * i | 0x80;
					if (k & 0xf0)
						j += 4;
					if (k & 0xcc)
						j += 2;
					if (k & 0xaa)
						j++;
					keyb_key = j;
				}
			}

			if (keyb_key && (!(keyb_matrix[1] & 0x80) || (!(keyb_matrix[6] & 0x10))))
				keyb_key |= 0x40;
		}
		else
		{
			keyb_matrix[0] = 0xff;
			keyb_matrix[1] = 0xff;
			keyb_matrix[2] = 0xff;
			keyb_matrix[3] = 0xff;
			keyb_matrix[4] = 0xff;
			keyb_matrix[5] = 0xff;
			keyb_matrix[6] = 0xff;
			keyb_matrix[7] = 0xff;
		}
	}

	cia1.pra = ciaa_pra_def;	
}
