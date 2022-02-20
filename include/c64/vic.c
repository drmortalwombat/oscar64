#include "vic.h"
#include "cia.h"

void vic_setbank(char bank)
{
	cia2.pra = (cia2.pra & 0xfc) | (bank ^ 0x03);
}

void vic_sprxy(byte s, int x, int y)
{
	vic.spr_pos[s].y = y;
	vic.spr_pos[s].x = x & 0xff;
	if (x & 0x100)
		vic.spr_msbx |= 1 << s;
	else
		vic.spr_msbx &= ~(1 << s);
}

void vic_setmode(VicMode mode, char * text, char * font)
{
	switch (mode)
	{
		case VICM_TEXT:
			vic.ctrl1 = VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3;
			vic.ctrl2 = VIC_CTRL2_CSEL;
			break;
		case VICM_TEXT_MC:
			vic.ctrl1 = VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3;
			vic.ctrl2 = VIC_CTRL2_CSEL | VIC_CTRL2_MCM;
			break;
		case VICM_TEXT_ECM:
			vic.ctrl1 = VIC_CTRL1_DEN | VIC_CTRL1_ECM | VIC_CTRL1_RSEL | 3;
			vic.ctrl2 = VIC_CTRL2_CSEL;
			break;
		case VICM_HIRES:
			vic.ctrl1 = VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3;
			vic.ctrl2 = VIC_CTRL2_CSEL;
			break;
		case VICM_HIRES_MC:
			vic.ctrl1 = VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3;
			vic.ctrl2 = VIC_CTRL2_CSEL | VIC_CTRL2_MCM;
			break;
		default:
			__assume(false)
	}

	cia2.pra = (cia2.pra & 0xfc) | (((unsigned)text >> 14) ^ 0x03);	
	vic.memptr = (((unsigned)text >> 6) & 0xf0) | (((unsigned)font >> 10) & 0x0e);
}

void vic_waitBottom(void)
{
	while (!(vic.ctrl1 & VIC_CTRL1_RST8))
		;
}

void vic_waitTop(void)
{
	while ((vic.ctrl1 & VIC_CTRL1_RST8))
		;
}

void vic_waitFrame(void)
{
	while ((vic.ctrl1 & VIC_CTRL1_RST8))
		;
	while (!(vic.ctrl1 & VIC_CTRL1_RST8))
		;	
}

void vic_waitLine(int line)
{
	char	upper = (char)(line >> 1) & VIC_CTRL1_RST8;
	char	lower = (char)line;

	do
	{
		while (vic.raster != lower)
			;
	} while ((vic.ctrl1 & VIC_CTRL1_RST8) != upper);
}

#pragma native(vic_waitLine)
