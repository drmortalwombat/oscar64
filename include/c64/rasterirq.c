#include "rasterirq.h"

#include <c64/vic.h>
#include <c64/cia.h>
#include <c64/asm6502.h>
#include <stdlib.h>

volatile byte rirq_count;
static byte rirq_pcount;

byte		rasterIRQRows[NUM_IRQS + 1];	
byte		rasterIRQIndex[NUM_IRQS + 1];	// Sort order of interrupt index, offset by one
#ifdef ZPAGE_IRQS
__zeropage
#endif
byte		rasterIRQNext[NUM_IRQS + 1];	// Rasterline of interrupt, terminated by 0xff
byte		rasterIRQLow[NUM_IRQS];			// Address of interrupt code
byte		rasterIRQHigh[NUM_IRQS];

#ifdef ZPAGE_IRQS
__zeropage
#endif
byte		nextIRQ;

// nextIRQ is the index of the next expected IRQ, or $ff if no IRQ is scheduled

__asm rirq_isr_ram_io
{	
	stx plrx + 1

	ldx	nextIRQ
	bmi	exi

	sta plra + 1
	sty plry + 1

l1:	
	lda	rasterIRQNext, x
	ldy rasterIRQIndex + 1, x
	ldx	rasterIRQLow, y
	stx ji + 1
	ldx	rasterIRQHigh, y
	stx ji + 2

ji:	
	jsr $0000

	inc	nextIRQ
	ldx nextIRQ

	ldy	rasterIRQNext, x

	asl $d019

	cpy #$ff
	beq e2

	dey
	sty	$d012
	dey
	cpy $d012
	bcc l1

plry:
	ldy #0
plra:
	lda #0
plrx:
	ldx #0
    rti

exi:
	asl $d019
	jmp plrx

    // No more interrupts to service
e2:
	inc rirq_count

	ldy	rasterIRQNext
	dey
	sty	$d012
	ldx	#0
	stx nextIRQ
	beq	plry
}

__asm rirq_isr_io
{	
    pha
    txa
    pha
    tya
    pha
kentry:

	ldx	nextIRQ
	bmi	exi
l1:
	lda	rasterIRQNext, x
	ldy rasterIRQIndex + 1, x
	ldx	rasterIRQLow, y
	stx ji + 1
	ldx	rasterIRQHigh, y
	stx ji + 2

ji:	
	jsr $0000

	inc	nextIRQ
	ldx nextIRQ

	ldy	rasterIRQNext, x

	asl $d019

	cpy #$ff
	beq e2

	dey
	sty	$d012
	dey
	cpy $d012
	bcc l1

exd:
    pla
    tay
    pla
    tax
    pla
    rti

exi:
	asl $d019
	jmp exd

e2:
	inc rirq_count

	ldy	rasterIRQNext
	dey
	sty	$d012
	ldx	#0
	stx nextIRQ
	beq	exd
}

__asm rirq_isr_noio
{	
    pha
    txa
    pha
    tya
    pha
kentry:
    lda $01
    pha

    lda #$35
    sta $01

	ldx	nextIRQ
	bmi	exi
l1:
	lda	rasterIRQNext, x
	ldy rasterIRQIndex + 1, x
	ldx	rasterIRQLow, y
	stx ji + 1
	ldx	rasterIRQHigh, y
	stx ji + 2

ji:	
	jsr $0000

	inc	nextIRQ
	ldx nextIRQ

	ldy	rasterIRQNext, x

	asl $d019

	cpy #$ff
	beq e2

	dey
	sty	$d012
	dey
	cpy $d012
	bcc l1

exd:
	pla
	sta $01

    pla
    tay
    pla
    tax
    pla
    rti

exi:
	asl $d019
	jmp exd

e2:
	inc rirq_count

	ldy	rasterIRQNext
	dey
	sty	$d012
	ldx	#0
	stx nextIRQ
	beq	exd
}

__asm rirq_isr_kernal_io
{	
	lda $d019
	bpl ex2
	
	ldx	nextIRQ
	bmi exi
l1:
	lda	rasterIRQNext, x
	ldy rasterIRQIndex + 1, x
	ldx	rasterIRQLow, y
	stx ji + 1
	ldx	rasterIRQHigh, y
	stx ji + 2

ji:	
	jsr $0000
jx:

	inc	nextIRQ
	ldx nextIRQ

	ldy	rasterIRQNext, x

	asl $d019

	cpy #$ff
	beq e2

	dey
	dey
	sty	$d012
	dey
	cpy $d012
	bcc l1

exd:
	jmp $ea81

exi:
	asl $d019
	jmp $ea81

e2:
	inc rirq_count

	ldy	rasterIRQNext
	dey
	dey
	sty	$d012
	ldx	#0
	stx nextIRQ
	jmp $ea81

ex2:
	LDA $DC0D
	cli
	jmp $ea31
}

__asm rirq_isr_kernal_noio
{	
	lda $01
	pha
	lda #$36
	sta $01

	lda $d019
	bpl ex2
	
	ldx	nextIRQ
	bmi exi
l1:
	lda	rasterIRQNext, x
	ldy rasterIRQIndex + 1, x
	ldx	rasterIRQLow, y
	stx ji + 1
	ldx	rasterIRQHigh, y
	stx ji + 2

ji:	
	jsr $0000
jx:

	inc	nextIRQ
	ldx nextIRQ

	ldy	rasterIRQNext, x

	asl $d019

	cpy #$ff
	beq e2

	dey
	dey
	sty	$d012
	dey
	cpy $d012
	bcc l1
exd:
	pla
	sta $01

	jmp $ea81

exi:
	asl $d019
	jmp exd

e2:
	inc rirq_count

	ldy	rasterIRQNext
	dey
	dey
	sty	$d012
	ldx	#0
	stx nextIRQ
	beq	exd

ex2:
	LDA $DC0D
	cli
	pla
	sta $01

	jmp $ea31
}

//  0 lda #data0
//  2 ldy #data1
//  4 cpx $d012
//  7 bcc -5
//  9 sta addr0
// 12 sty addr1
// 15 lda #data2
// 17 sta addr2
// 20 lda #data3
// 22 sta addr3
// ...
//   rts

void rirq_build(RIRQCode * ic, byte size)
{
	__assume(size < 26);

	ic->size = size;

	asm_im(ic->code + 0, ASM_LDY, 0);
	asm_im(ic->code + 2, ASM_LDX, 0);
	asm_ab(ic->code + 4, ASM_CMP, 0xd012);
	asm_rl(ic->code + 7, ASM_BCS, -5);
	asm_ab(ic->code + 9, ASM_STY, 0x0000);

	if (size == 0)
	{
		asm_np(ic->code + 0, ASM_RTS);
	}
	else if (size == 1)
	{
		asm_np(ic->code + 12, ASM_RTS);
	}
	else
	{
		asm_ab(ic->code + 12, ASM_STX, 0x0000);

		byte p = 15;
		for(byte i=2; i<size; i++)
		{
			p += asm_im(ic->code + p, ASM_LDA, 0x00);
			p += asm_ab(ic->code + p, ASM_STA, 0x0000);
		}
		asm_np(ic->code + p, ASM_RTS);
	}
}

RIRQCode * rirq_alloc(byte size)
{
	RIRQCode	* ic = (RIRQCode *)malloc(1 + RIRQ_SIZE + 5 * size);
	rirq_build(ic, size);
	return ic;
}

#pragma native(rirq_build)

void rirq_set(byte n, byte row, RIRQCode * write)
{
	rasterIRQLow[n] = (unsigned)&write->code & 0xff;
	rasterIRQHigh[n] = (unsigned)&write->code >> 8;

	rasterIRQRows[n] = row;
}

static const byte irqai[26] = {
	RIRQ_ADDR_0, RIRQ_ADDR_1, RIRQ_ADDR_2, RIRQ_ADDR_3, RIRQ_ADDR_4, RIRQ_ADDR_5, RIRQ_ADDR_6, RIRQ_ADDR_7,
	RIRQ_ADDR_8, RIRQ_ADDR_9, RIRQ_ADDR_10, RIRQ_ADDR_11, RIRQ_ADDR_12, RIRQ_ADDR_13, RIRQ_ADDR_14, RIRQ_ADDR_15,
	RIRQ_ADDR_16, RIRQ_ADDR_17, RIRQ_ADDR_18, RIRQ_ADDR_19, RIRQ_ADDR_20, RIRQ_ADDR_21, RIRQ_ADDR_22, RIRQ_ADDR_23,
	RIRQ_ADDR_24, RIRQ_ADDR_25
};

static const byte irqdi[26] = {
	RIRQ_DATA_0, RIRQ_DATA_1, RIRQ_DATA_2, RIRQ_DATA_3, RIRQ_DATA_4, RIRQ_DATA_5, RIRQ_DATA_6, RIRQ_DATA_7,
	RIRQ_DATA_8, RIRQ_DATA_9, RIRQ_DATA_10, RIRQ_DATA_11, RIRQ_DATA_12, RIRQ_DATA_13, RIRQ_DATA_14, RIRQ_DATA_15,
	RIRQ_DATA_16, RIRQ_DATA_17, RIRQ_DATA_18, RIRQ_DATA_19, RIRQ_DATA_20, RIRQ_DATA_21, RIRQ_DATA_22, RIRQ_DATA_23,
	RIRQ_DATA_24, RIRQ_DATA_25
};

void rirq_addr(RIRQCode * ic, byte n, void * addr)
{
	byte p = irqai[n];
	ic->code[p + 0] = (unsigned)addr & 0xff;
	ic->code[p + 1] = (unsigned)addr >> 8;
}

void rirq_addrhi(RIRQCode * ic, byte n, byte hi)
{
	byte p = irqai[n];
	ic->code[p + 1] = hi;	
}

void rirq_data(RIRQCode * ic, byte n, byte data)
{
	byte p = irqdi[n];
//	ic->code[p] = data;	
	(volatile char *)(ic->code)[p] = data;
}

void rirq_write(RIRQCode * ic, byte n, void * addr, byte data)
{
	byte p = irqai[n];
	ic->code[p + 0] = (unsigned)addr & 0xff;
	ic->code[p + 1] = (unsigned)addr >> 8;
	p = irqdi[n];
	ic->code[p] = data;
}

void rirq_call(RIRQCode * ic, byte n, void * addr)
{
	byte p = irqai[n];
	ic->code[p - 1] = 0x20;
	ic->code[p + 0] = (unsigned)addr & 0xff;
	ic->code[p + 1] = (unsigned)addr >> 8;
}

void rirq_delay(RIRQCode * ic, byte cycles)
{
	ic->code[ 1]  = cycles;
	ic->code[ 9] = 0x88; // dey
	ic->code[10] = 0xd0; // bne
	ic->code[11] = 0xfd; // -3
}


void rirq_move(byte n, byte row)
{
	rasterIRQRows[n] = row;
}

void rirq_clear(byte n)
{
	rasterIRQRows[n] = 255;
}

void rirq_init_tables(void)
{
	for(byte i=0; i<NUM_IRQS; i++)
	{
		rasterIRQRows[i] = 255;
		rasterIRQIndex[i + 1] = i;
	}
	rasterIRQIndex[0] = NUM_IRQS;
	rasterIRQRows[NUM_IRQS] = 0;
	rasterIRQNext[NUM_IRQS] = 255;
}

void rirq_init_kernal(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0x0314 = rirq_isr_kernal_io;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init_kernal_noio(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0x0314 = rirq_isr_kernal_noio;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init_crt(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0x0314 = rirq_isr_io.kentry;
   	*(void **)0xfffe = rirq_isr_io;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init_crt_noio(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0x0314 = rirq_isr_noio.kentry;
   	*(void **)0xfffe = rirq_isr_noio;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init_io(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0xfffe = rirq_isr_ram_io;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init_memmap(void)
{
	rirq_init_tables();

    __asm 
    {
        sei
    }

   	*(void **)0xfffe = rirq_isr_noio;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;

}

void rirq_init(bool kernalIRQ)
{
	if (kernalIRQ)
		rirq_init_kernal();
	else
		rirq_init_io();
}

void rirq_wait(void)
{
	char i0 = rirq_pcount;
	char i1;
	do {
		i1 = rirq_count;
	} while (i0 == i1);
	rirq_pcount = i1;
}

void rirq_sort(bool inirq)
{
	// disable raster interrupts while sorting
	nextIRQ = 0xff;
#if 1
	byte maxr = rasterIRQRows[rasterIRQIndex[1]];
	for(byte i = 2; i<NUM_IRQS + 1; i++)
	{
		byte ri = rasterIRQIndex[i];
		byte rr = rasterIRQRows[ri];
		if (rr < maxr)
		{
			rasterIRQIndex[i] = rasterIRQIndex[i - 1];
			byte j = i, rj;
			while (rr < rasterIRQRows[(rj = rasterIRQIndex[j - 2])])
			{
				rasterIRQIndex[j - 1] = rj;
				j--;
			}
			rasterIRQIndex[j - 1] = ri;
		}
		else
			maxr = rr;
	}
#else
	for(byte i = 1; i<NUM_IRQS; i++)
	{
		byte ri = rasterIRQIndex[i];
		byte rr = rasterIRQRows[ri];
		byte j = i, rj;
		while (j > 0 && rr < rasterIRQRows[(rj = rasterIRQIndex[j - 1])])
		{
			rasterIRQIndex[j] = rj;
			j--;
		}
		rasterIRQIndex[j] = ri;
	}
#endif

#if NUM_IRQS & 3
	for(sbyte i=NUM_IRQS-1; i>=0; i--)
		rasterIRQNext[i] = rasterIRQRows[rasterIRQIndex[i + 1]];
#else
	for(sbyte i=NUM_IRQS/4-1; i>=0; i--)
	{
		#pragma unroll(full)
		for(int j=0; j<4; j++)
			rasterIRQNext[i + j * NUM_IRQS / 4] = rasterIRQRows[rasterIRQIndex[i + j * NUM_IRQS / 4 + 1]];
	}
#endif

	rirq_pcount = rirq_count;
	if (inirq)
		nextIRQ = NUM_IRQS - 1;
	else
	{
		byte	yp = rasterIRQNext[0];
		if (yp != 0xff)
		{
			vic.raster = yp - 1;
			nextIRQ = 0;
		}
	}
}

void rirq_start(void)
{
    __asm 
    {
        lda $d011
        and #$7f
        sta $d011

        lda #100
        sta $d012

        asl $d019
        cli
    }
}

void rirq_stop(void)
{
    __asm 
    {
        sei
    }
}

#pragma native(rirq_sort)
#pragma native(rirq_wait)
#pragma native(rirq_start)
#pragma native(rirq_stop)

