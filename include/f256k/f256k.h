#ifndef F256K_H
#define F256K_H

// F256K Hardware Register Definitions for oscar64
// Based on the F256 Hardware Reference Manual

typedef unsigned char	byte;
typedef unsigned int	word;
typedef unsigned long	dword;

typedef signed char	sbyte;

// ============================================================
// MMU Registers
// ============================================================

struct MMU
{
	volatile byte	mem_ctrl;
	volatile byte	io_ctrl;
};

#define mmu	(*((struct MMU *)0x0000))

// MMU_MEM_CTRL bits
#define MMU_EDIT_EN     0x80    // Enable MLUT editing
#define MMU_EDIT_LUT0   0x00    // Edit MLUT 0
#define MMU_EDIT_LUT1   0x10    // Edit MLUT 1
#define MMU_EDIT_LUT2   0x20    // Edit MLUT 2
#define MMU_EDIT_LUT3   0x30    // Edit MLUT 3
#define MMU_ACT_LUT0    0x00    // Activate MLUT 0
#define MMU_ACT_LUT1    0x01    // Activate MLUT 1
#define MMU_ACT_LUT2    0x02    // Activate MLUT 2
#define MMU_ACT_LUT3    0x03    // Activate MLUT 3

// MMU_IO_CTRL bits
#define MMU_IO_DISABLE  0x04    // Disable I/O, Bank 6 = RAM
#define MMU_IO_PAGE0    0x00    // I/O page 0 (control registers)
#define MMU_IO_PAGE1    0x01    // I/O page 1 (font memory)
#define MMU_IO_PAGE2    0x02    // I/O page 2 (text character matrix)
#define MMU_IO_PAGE3    0x03    // I/O page 3 (text color matrix)

// MLUT editing registers (visible when MMU_EDIT_EN set)
#define MMU_LUT         ((volatile char *)0x0008)

// ============================================================
// VICKY (TinyVicky) Master Control Registers
// ============================================================

struct VKY
{
	volatile byte	ctrl0;          // 0xD000
	volatile byte	ctrl1;          // 0xD001
	byte		_pad0[2];       // 0xD002-0xD003
	volatile byte	brd_ctrl;       // 0xD004
	volatile byte	brd_color_b;    // 0xD005
	volatile byte	brd_color_g;    // 0xD006
	volatile byte	brd_color_r;    // 0xD007
	volatile byte	brd_x_size;     // 0xD008
	volatile byte	brd_y_size;     // 0xD009
	byte		_pad1[3];       // 0xD00A-0xD00C
	volatile byte	bkg_color_b;    // 0xD00D
	volatile byte	bkg_color_g;    // 0xD00E
	volatile byte	bkg_color_r;    // 0xD00F
	volatile byte	crsr_ctrl;      // 0xD010
	byte		_pad2;          // 0xD011
	volatile byte	crsr_char;      // 0xD012
	volatile byte	crsr_color;     // 0xD013
	volatile word	crsr_x;         // 0xD014-0xD015
	volatile word	crsr_y;         // 0xD016-0xD017
};

#define vky	(*((struct VKY *)0xd000))

// VKY_MSTR_CTRL_0 bits
#define VKY_TEXT        0x01    // Enable text display
#define VKY_OVRLY       0x02    // Overlay text on graphics
#define VKY_GRAPH       0x04    // Enable graphics modes
#define VKY_BITMAP      0x08    // Enable bitmap display
#define VKY_TILE        0x10    // Enable tile display
#define VKY_SPRITE      0x20    // Enable sprite display
#define VKY_GAMMA       0x40    // Enable gamma correction

// VKY_MSTR_CTRL_1 bits
#define VKY_CLK_70      0x01    // 70Hz mode (vs 60Hz)
#define VKY_DBL_X       0x02    // Double character width
#define VKY_DBL_Y       0x04    // Double character height
#define VKY_MON_SLP     0x08    // Monitor sleep
#define VKY_FON_OVLY    0x10    // Font overlay mode
#define VKY_FON_SET     0x20    // Font set selection (0 or 1)

// Text display memory (I/O page 2)
#define VKY_TEXT_MEM    ((volatile char *)0xc000)

// Text color memory (I/O page 3)
#define VKY_COLOR_MEM   ((volatile char *)0xc000)

// Text color LUTs (I/O page 0)
#define VKY_TXT_FG_LUT  ((volatile char *)0xd800)
#define VKY_TXT_BG_LUT  ((volatile char *)0xd840)

// Font memory (I/O page 1)
#define VKY_FONT0       ((volatile char *)0xc000)
#define VKY_FONT1       ((volatile char *)0xc800)

// ============================================================
// Interrupt Controller
// ============================================================

struct IntCtrl
{
	volatile byte	pending[3];     // 0xD660-0xD662
	byte		_pad0;          // 0xD663
	volatile byte	polarity[3];    // 0xD664-0xD666
	byte		_pad1;          // 0xD667
	volatile byte	edge[3];        // 0xD668-0xD66A
	byte		_pad2;          // 0xD66B
	volatile byte	mask[3];        // 0xD66C-0xD66E
};

#define intctrl	(*((struct IntCtrl *)0xd660))

// Interrupt Group 0 bits
#define INT_VKY_SOF     0x01    // Start Of Frame
#define INT_VKY_SOL     0x02    // Start Of Line
#define INT_PS2_KBD     0x04    // PS/2 keyboard
#define INT_PS2_MOUSE   0x08    // PS/2 mouse
#define INT_TIMER_0     0x10    // Timer 0
#define INT_TIMER_1     0x20    // Timer 1
#define INT_CARTRIDGE   0x80    // Cartridge

// Interrupt Group 1 bits
#define INT_UART        0x01    // UART
#define INT_RTC         0x10    // Real time clock
#define INT_VIA0        0x20    // VIA 0
#define INT_VIA1        0x40    // VIA 1 (F256K keyboard)
#define INT_SDC_INS     0x80    // SD card insertion

// ============================================================
// PS/2 Interface
// ============================================================

struct PS2
{
	volatile byte	ctrl;           // 0xD640
	volatile byte	out;            // 0xD641
	volatile byte	kbd_in;         // 0xD642
	volatile byte	ms_in;          // 0xD643
	volatile byte	stat;           // 0xD644
};

#define ps2	(*((struct PS2 *)0xd640))

// PS2 ctrl bits
#define PS2_K_WR        0x01    // Keyboard write
#define PS2_M_WR        0x02    // Mouse write
#define PS2_KCLR        0x04    // Clear keyboard FIFO
#define PS2_MCLR        0x08    // Clear mouse FIFO

// PS2 stat bits
#define PS2_KEMP        0x01    // Keyboard FIFO empty
#define PS2_MEMP        0x02    // Mouse FIFO empty

// ============================================================
// Timers
// ============================================================

struct Timer
{
	volatile byte	ctrl;           // +0x00
	volatile byte	value[3];       // +0x01-0x03
	volatile byte	cmp[3];         // +0x04-0x06
	byte		_pad;           // +0x07
};

#define timer0	(*((struct Timer *)0xd650))
#define timer1	(*((struct Timer *)0xd658))

// Timer control bits
#define TIMER_EN        0x01    // Enable timer
#define TIMER_CLR       0x02    // Clear timer
#define TIMER_LOAD      0x04    // Load compare value
#define TIMER_UP        0x08    // Count up (vs down)
#define TIMER_SCLR      0x10    // Auto-clear on compare

// ============================================================
// UART (16750-compatible)
// ============================================================

struct UART
{
	volatile byte	data;           // 0xD630 (RXD/TXR, or DLL when DLAB=1)
	volatile byte	ier;            // 0xD631 (IER, or DLH when DLAB=1)
	volatile byte	isr;            // 0xD632 (ISR read / FCR write)
	volatile byte	lcr;            // 0xD633
	volatile byte	mcr;            // 0xD634
	volatile byte	lsr;            // 0xD635
	volatile byte	msr;            // 0xD636
	volatile byte	spr;            // 0xD637
};

#define uart	(*((struct UART *)0xd630))

// LSR bits
#define UART_LSR_DR     0x01    // Data ready
#define UART_LSR_THRE   0x20    // Transmitter holding register empty

// ============================================================
// DMA Controller
// ============================================================

struct DMA
{
	volatile byte	ctrl;           // 0xDF00
	volatile byte	fill_byte;      // 0xDF01
	byte		_pad0[2];       // 0xDF02-0xDF03
	volatile byte	src_addr[3];    // 0xDF04-0xDF06
	byte		_pad1;          // 0xDF07
	volatile byte	dst_addr[3];    // 0xDF08-0xDF0A
	byte		_pad2;          // 0xDF0B
	volatile word	count;          // 0xDF0C-0xDF0D
	volatile word	height;         // 0xDF0E-0xDF0F
	volatile word	src_stride;     // 0xDF10-0xDF11
	volatile word	dst_stride;     // 0xDF12-0xDF13
};

#define dma	(*((struct DMA *)0xdf00))

// DMA ctrl bits
#define DMA_ENABLE      0x01    // Enable DMA
#define DMA_2D          0x02    // 2D operation
#define DMA_FILL        0x04    // Fill (vs copy)
#define DMA_INT_EN      0x08    // Interrupt on complete
#define DMA_START       0x80    // Start transfer

// ============================================================
// System Control
// ============================================================

struct SysCtrl
{
	volatile byte	sys0;           // 0xD6A0
	volatile byte	sys1;           // 0xD6A1
	volatile byte	rst0;           // 0xD6A2
	volatile byte	rst1;           // 0xD6A3
	volatile byte	rnd_lo;         // 0xD6A4
	volatile byte	rnd_hi;         // 0xD6A5
	volatile byte	rnd_ctrl;       // 0xD6A6
	volatile byte	mid;            // 0xD6A7
};

#define sysctl	(*((struct SysCtrl *)0xd6a0))

// SYS0 bits
#define SYS_PWR_LED     0x01    // Power LED
#define SYS_SD_LED      0x02    // SD card LED
#define SYS_BUZZ        0x10    // Buzzer control
#define SYS_CAP_EN      0x20    // Capslock LED (F256K)
#define SYS_RESET       0x80    // Software reset

// RNG control bits
#define SYS_RNG_EN      0x01    // Enable RNG
#define SYS_RNG_SEED    0x02    // Load seed

// Machine IDs
#define MID_F256JR      0x02
#define MID_F256K       0x12

// ============================================================
// VIA (65C22) Registers
// ============================================================

struct VIA
{
	volatile byte	iorb;           // +0x00
	volatile byte	iora;           // +0x01
	volatile byte	ddrb;           // +0x02
	volatile byte	ddra;           // +0x03
	volatile word	t1c;            // +0x04-0x05
	volatile word	t1l;            // +0x06-0x07
	volatile word	t2c;            // +0x08-0x09
	volatile byte	sdr;            // +0x0A
	volatile byte	acr;            // +0x0B
	volatile byte	pcr;            // +0x0C
	volatile byte	ifr;            // +0x0D
	volatile byte	ier;            // +0x0E
	volatile byte	iora2;          // +0x0F
};

// VIA 0 (auxiliary I/O)
#define via0	(*((struct VIA *)0xdc00))

// VIA 1 (keyboard, F256K only)
#define via1	(*((struct VIA *)0xdb00))

// ============================================================
// SD Card Controller
// ============================================================

#define SDC_BASE        ((volatile char *)0xdd00)

// ============================================================
// Integer Math Coprocessor
// ============================================================

#define MATH_BASE       ((volatile char *)0xde00)

#endif // F256K_H
