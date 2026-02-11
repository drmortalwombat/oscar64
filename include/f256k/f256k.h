#ifndef F256K_H
#define F256K_H

// F256K Hardware Register Definitions for oscar64
// Based on the F256 Hardware Reference Manual

// ============================================================
// MMU Registers
// ============================================================

#define MMU_MEM_CTRL    (*(volatile char *)0x0000)
#define MMU_IO_CTRL     (*(volatile char *)0x0001)

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

#define VKY_MSTR_CTRL_0 (*(volatile char *)0xd000)
#define VKY_MSTR_CTRL_1 (*(volatile char *)0xd001)

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

// Border control
#define VKY_BRD_CTRL    (*(volatile char *)0xd004)
#define VKY_BRD_COLOR_B (*(volatile char *)0xd005)
#define VKY_BRD_COLOR_G (*(volatile char *)0xd006)
#define VKY_BRD_COLOR_R (*(volatile char *)0xd007)
#define VKY_BRD_X_SIZE  (*(volatile char *)0xd008)
#define VKY_BRD_Y_SIZE  (*(volatile char *)0xd009)

// Background color
#define VKY_BKG_COLOR_B (*(volatile char *)0xd00d)
#define VKY_BKG_COLOR_G (*(volatile char *)0xd00e)
#define VKY_BKG_COLOR_R (*(volatile char *)0xd00f)

// Cursor control
#define VKY_CRSR_CTRL   (*(volatile char *)0xd010)
#define VKY_CRSR_CHAR   (*(volatile char *)0xd012)
#define VKY_CRSR_COLOR  (*(volatile char *)0xd013)
#define VKY_CRSR_X_L    (*(volatile char *)0xd014)
#define VKY_CRSR_X_H    (*(volatile char *)0xd015)
#define VKY_CRSR_Y_L    (*(volatile char *)0xd016)
#define VKY_CRSR_Y_H    (*(volatile char *)0xd017)

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

#define INT_PENDING_0   (*(volatile char *)0xd660)
#define INT_PENDING_1   (*(volatile char *)0xd661)
#define INT_PENDING_2   (*(volatile char *)0xd662)

#define INT_POLARITY_0  (*(volatile char *)0xd664)
#define INT_POLARITY_1  (*(volatile char *)0xd665)
#define INT_POLARITY_2  (*(volatile char *)0xd666)

#define INT_EDGE_0      (*(volatile char *)0xd668)
#define INT_EDGE_1      (*(volatile char *)0xd669)
#define INT_EDGE_2      (*(volatile char *)0xd66a)

#define INT_MASK_0      (*(volatile char *)0xd66c)
#define INT_MASK_1      (*(volatile char *)0xd66d)
#define INT_MASK_2      (*(volatile char *)0xd66e)

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

#define PS2_CTRL        (*(volatile char *)0xd640)
#define PS2_OUT         (*(volatile char *)0xd641)
#define KBD_IN          (*(volatile char *)0xd642)
#define MS_IN           (*(volatile char *)0xd643)
#define PS2_STAT        (*(volatile char *)0xd644)

// PS2_CTRL bits
#define PS2_K_WR        0x01    // Keyboard write
#define PS2_M_WR        0x02    // Mouse write
#define PS2_KCLR        0x04    // Clear keyboard FIFO
#define PS2_MCLR        0x08    // Clear mouse FIFO

// PS2_STAT bits
#define PS2_KEMP        0x01    // Keyboard FIFO empty
#define PS2_MEMP        0x02    // Mouse FIFO empty

// ============================================================
// Timers
// ============================================================

#define TIMER0_CTRL     (*(volatile char *)0xd650)
#define TIMER0_VALUE    ((volatile char *)0xd651)
#define TIMER0_CMP      ((volatile char *)0xd654)

#define TIMER1_CTRL     (*(volatile char *)0xd658)
#define TIMER1_VALUE    ((volatile char *)0xd659)
#define TIMER1_CMP      ((volatile char *)0xd65c)

// Timer control bits
#define TIMER_EN        0x01    // Enable timer
#define TIMER_CLR       0x02    // Clear timer
#define TIMER_LOAD      0x04    // Load compare value
#define TIMER_UP        0x08    // Count up (vs down)
#define TIMER_SCLR      0x10    // Auto-clear on compare

// ============================================================
// UART (16750-compatible)
// ============================================================

#define UART_RXD        (*(volatile char *)0xd630)
#define UART_TXR        (*(volatile char *)0xd630)
#define UART_IER        (*(volatile char *)0xd631)
#define UART_ISR        (*(volatile char *)0xd632)
#define UART_FCR        (*(volatile char *)0xd632)
#define UART_LCR        (*(volatile char *)0xd633)
#define UART_MCR        (*(volatile char *)0xd634)
#define UART_LSR        (*(volatile char *)0xd635)
#define UART_MSR        (*(volatile char *)0xd636)
#define UART_SPR        (*(volatile char *)0xd637)

// Baud rate divisor (DLAB=1)
#define UART_DLL        (*(volatile char *)0xd630)
#define UART_DLH        (*(volatile char *)0xd631)

// LSR bits
#define UART_LSR_DR     0x01    // Data ready
#define UART_LSR_THRE   0x20    // Transmitter holding register empty

// ============================================================
// DMA Controller
// ============================================================

#define DMA_CTRL        (*(volatile char *)0xdf00)
#define DMA_FILL_BYTE   (*(volatile char *)0xdf01)
#define DMA_SRC_ADDR    ((volatile char *)0xdf04)
#define DMA_DST_ADDR    ((volatile char *)0xdf08)
#define DMA_COUNT       ((volatile char *)0xdf0c)
#define DMA_HEIGHT      ((volatile char *)0xdf0e)
#define DMA_SRC_STRIDE  ((volatile char *)0xdf10)
#define DMA_DST_STRIDE  ((volatile char *)0xdf12)

// DMA_CTRL bits
#define DMA_ENABLE      0x01    // Enable DMA
#define DMA_2D          0x02    // 2D operation
#define DMA_FILL        0x04    // Fill (vs copy)
#define DMA_INT_EN      0x08    // Interrupt on complete
#define DMA_START       0x80    // Start transfer

// ============================================================
// System Control
// ============================================================

#define SYS0            (*(volatile char *)0xd6a0)
#define SYS1            (*(volatile char *)0xd6a1)
#define SYS_RST0        (*(volatile char *)0xd6a2)
#define SYS_RST1        (*(volatile char *)0xd6a3)
#define SYS_RNDL        (*(volatile char *)0xd6a4)
#define SYS_RNDH        (*(volatile char *)0xd6a5)
#define SYS_RND_CTRL    (*(volatile char *)0xd6a6)
#define SYS_MID         (*(volatile char *)0xd6a7)

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

// VIA 0 (auxiliary I/O)
#define VIA0_IORB       (*(volatile char *)0xdc00)
#define VIA0_IORA       (*(volatile char *)0xdc01)
#define VIA0_DDRB       (*(volatile char *)0xdc02)
#define VIA0_DDRA       (*(volatile char *)0xdc03)
#define VIA0_T1C_L      (*(volatile char *)0xdc04)
#define VIA0_T1C_H      (*(volatile char *)0xdc05)
#define VIA0_T1L_L      (*(volatile char *)0xdc06)
#define VIA0_T1L_H      (*(volatile char *)0xdc07)
#define VIA0_T2C_L      (*(volatile char *)0xdc08)
#define VIA0_T2C_H      (*(volatile char *)0xdc09)
#define VIA0_SDR        (*(volatile char *)0xdc0a)
#define VIA0_ACR        (*(volatile char *)0xdc0b)
#define VIA0_PCR        (*(volatile char *)0xdc0c)
#define VIA0_IFR        (*(volatile char *)0xdc0d)
#define VIA0_IER        (*(volatile char *)0xdc0e)
#define VIA0_IORA2      (*(volatile char *)0xdc0f)

// VIA 1 (keyboard, F256K only)
#define VIA1_IORB       (*(volatile char *)0xdb00)
#define VIA1_IORA       (*(volatile char *)0xdb01)
#define VIA1_DDRB       (*(volatile char *)0xdb02)
#define VIA1_DDRA       (*(volatile char *)0xdb03)
#define VIA1_T1C_L      (*(volatile char *)0xdb04)
#define VIA1_T1C_H      (*(volatile char *)0xdb05)
#define VIA1_T1L_L      (*(volatile char *)0xdb06)
#define VIA1_T1L_H      (*(volatile char *)0xdb07)
#define VIA1_T2C_L      (*(volatile char *)0xdb08)
#define VIA1_T2C_H      (*(volatile char *)0xdb09)
#define VIA1_SDR        (*(volatile char *)0xdb0a)
#define VIA1_ACR        (*(volatile char *)0xdb0b)
#define VIA1_PCR        (*(volatile char *)0xdb0c)
#define VIA1_IFR        (*(volatile char *)0xdb0d)
#define VIA1_IER        (*(volatile char *)0xdb0e)
#define VIA1_IORA2      (*(volatile char *)0xdb0f)

// ============================================================
// SD Card Controller
// ============================================================

#define SDC_BASE        ((volatile char *)0xdd00)

// ============================================================
// Integer Math Coprocessor
// ============================================================

#define MATH_BASE       ((volatile char *)0xde00)

#endif // F256K_H
