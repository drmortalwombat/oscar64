#include "ted.h"

void ted_waitBottom(void)
{
	while (!(ted.vscan_high & 1))
		;
}

void ted_waitTop(void)
{
	while (ted.vscan_high & 1)
		;
}

void ted_waitFrame(void)
{
	while (ted.vscan_high & 1)
		;
	while (!(ted.vscan_high & 1))
		;	
}

void ted_waitLine(int line)
{
	char	upper = (char)(line >> 8) & 1;
	char	lower = (char)line;

	do
	{
		while (ted.vscan_low != lower)
			;
	} while ((ted.vscan_high & 1) != upper);
}

void ted_setmode(TedMode mode, char * text, char * font)
{
	switch (mode)
	{
		case TEDM_TEXT:
			ted.ctrl1 = TED_CTRL1_DEN | TED_CTRL1_RSEL | 3;
			ted.ctrl2 = TED_CTRL2_CSEL;
			break;
		case TEDM_TEXT_MC:
			ted.ctrl1 = TED_CTRL1_DEN | TED_CTRL1_RSEL | 3;
			ted.ctrl2 = TED_CTRL2_CSEL | TED_CTRL2_MCM;
			break;
		case TEDM_TEXT_ECM:
			ted.ctrl1 = TED_CTRL1_DEN | TED_CTRL1_ECM | TED_CTRL1_RSEL | 3;
			ted.ctrl2 = TED_CTRL2_CSEL;
			break;
		case TEDM_HIRES:
			ted.ctrl1 = TED_CTRL1_BMM | TED_CTRL1_DEN | TED_CTRL1_RSEL | 3;
			ted.ctrl2 = TED_CTRL2_CSEL;
			break;
		case TEDM_HIRES_MC:
			ted.ctrl1 = TED_CTRL1_BMM | TED_CTRL1_DEN | TED_CTRL1_RSEL | 3;
			ted.ctrl2 = TED_CTRL2_CSEL | TED_CTRL2_MCM;
			break;
		default:
			__assume(false);
	}

	ted.vid_ptr = (unsigned)text >> 8;

	if (mode < TEDM_HIRES)
	{
		ted.char_ptr = (unsigned)font >> 8;
	}
	else
	{
		ted.sound1_high = (ted.sound1_high & 0x3) | ((unsigned)font >> 10);
	}
}
