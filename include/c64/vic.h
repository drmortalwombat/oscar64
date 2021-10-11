#ifndef C64_VIC_H
#define C64_VIC_H

#include "types.h"

#define VIC_CTRL1_RSEL	0x08
#define VIC_CTRL1_DEN	0x10
#define VIC_CTRL1_BMM	0x20
#define VIC_CTRL1_ECM	0x40
#define VIC_CTRL1_RST8	0x80

#define VIC_CTRL2_CSEL	0x10
#define VIC_CTRL2_MCM	0x20
#define VIC_CTRL2_RES	0x40

#define VIC_INTR_RST	0x01
#define VIC_INTR_MBC	0x02
#define VIC_INTR_MMC	0x04
#define VIC_INTR_ILP	0x08
#define VIC_INTR_IRQ	0x80

enum VICColors
{
	VCOL_BLACK,
	VCOL_WHITE,
	BCOL_RED,
	BCOL_CYAN,
	BCOL_PURPLE,
	BCOL_GREEN,
	BCOL_BLUE,
	BCOL_YELLOW,

	BCOL_ORANGE,
	BCOL_BROWN,
	BCOL_LT_RED,
	BCOL_DARK_GREY,
	BCOL_MED_GREY,
	BCOL_LT_GREEN,
	BCOL_LT_BLUE,
	BCOL_LT_GREY
};

struct VIC
{
	struct XY
	{
		byte	x, y;
	}	spritexy[8];
	byte	spr_msbx;

	byte	ctrl1;
	byte	raster;
	byte	lpx, lpy;
	byte	spr_enable;
	byte	ctrl2;
	byte	spr_expand_y;
	byte	memptr;
	byte	intr_ctrl;
	byte	intr_enable;
	byte	spr_priority;
	byte	spr_multi;
	byte	spr_expand_x;
	byte	spr_sprcol;
	byte	spr_backcol;
	byte	color_border;
	byte	color_back;
	byte	color_back1;
	byte	color_back2;
	byte	color_back3;
	byte	spr_mcolor0;
	byte	spr_mcolor1;
	byte	spr_color[8];

};

#define vic	(*((VIC *)0xd000))

#endif
