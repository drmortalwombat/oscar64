#include "rasterirq.h"

#include <c64/vic.h>
#include <c64/cia.h>
#include <c64/asm6502.h>
#include <stdlib.h>

volatile char npos = 1, tpos = 0;
volatile byte rirq_count;

byte		rasterIRQRows[NUM_IRQS];
byte		rasterIRQIndex[NUM_IRQS];
byte		rasterIRQNext[NUM_IRQS];
byte		rasterIRQLow[NUM_IRQS];
byte		rasterIRQHigh[NUM_IRQS];

byte		nextIRQ;

__asm irq0
{	
    pha
    txa
    pha
    tya
    pha

	ldx	nextIRQ
l1:
	lda	rasterIRQNext, x
	cmp	#$ff
	beq	e1

	ldy rasterIRQIndex, x
	tax
	lda	rasterIRQLow, y
	sta ji + 1
	lda	rasterIRQHigh, y
	sta ji + 2

ji:	
	jsr $0000
jx:

	inc	nextIRQ
	ldx nextIRQ

	lda	rasterIRQNext, x
	cmp #$ff
	beq e2

	tay

	sec
	sbc	#3
	cmp	$d012
	bcc l1

	dey
	sty	$d012
w1:
	jmp ex

e2:
	ldx npos
	stx tpos
	inc rirq_count

	bit	$d011
	bmi e1

	sta $d012

	asl $d019
	jmp ex

e1:
	ldx	#0
	stx nextIRQ
	lda	rasterIRQNext, x
	sec
	sbc	#1
	sta $d012
	
ex:
	asl $d019

    pla
    tay
    pla
    tax
    pla
    rti

}

__asm irq1
{	
	lda $d019
	bpl ex2
	
	ldx	nextIRQ
l1:
	lda	rasterIRQNext, x
	cmp	#$ff
	beq	e1

	ldy rasterIRQIndex, x
	tax
	lda	rasterIRQLow, y
	sta ji + 1
	lda	rasterIRQHigh, y
	sta ji + 2

ji:	
	jsr $0000
jx:

	inc	nextIRQ
	ldx nextIRQ

	lda	rasterIRQNext, x
	cmp #$ff
	beq e2

	tay

	sec
	sbc	#3
	cmp	$d012
	bcc l1

	dey
	sty	$d012
w1:
	jmp ex

e2:
	ldx npos
	stx tpos
	inc rirq_count

	bit	$d011
	bmi e1

	sta $d012

	jmp ex

e1:
	ldx	#0
	stx nextIRQ
	lda	rasterIRQNext, x
	sec
	sbc	#1
	sta $d012
	
ex:
	asl $d019

	jmp $ea81

ex2:
	LDA $DC0D
	cli
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
	ic->size = size;

	asm_im(ic->code + 0, ASM_LDY, 0);
	asm_im(ic->code + 2, ASM_LDA, 0);
	asm_ab(ic->code + 4, ASM_CPX, 0xd012);
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
		asm_ab(ic->code + 12, ASM_STA, 0x0000);

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

static const byte irqai[20] = {
	RIRQ_ADDR_0, RIRQ_ADDR_1, RIRQ_ADDR_2, RIRQ_ADDR_3, RIRQ_ADDR_4, RIRQ_ADDR_5, RIRQ_ADDR_6, RIRQ_ADDR_7,
	RIRQ_ADDR_8, RIRQ_ADDR_9, RIRQ_ADDR_10, RIRQ_ADDR_11, RIRQ_ADDR_12, RIRQ_ADDR_13, RIRQ_ADDR_14, RIRQ_ADDR_15,
	RIRQ_ADDR_16, RIRQ_ADDR_17, RIRQ_ADDR_18, RIRQ_ADDR_19
};

static const byte irqdi[20] = {
	RIRQ_DATA_0, RIRQ_DATA_1, RIRQ_DATA_2, RIRQ_DATA_3, RIRQ_DATA_4, RIRQ_DATA_5, RIRQ_DATA_6, RIRQ_DATA_7,
	RIRQ_DATA_8, RIRQ_DATA_9, RIRQ_DATA_10, RIRQ_DATA_11, RIRQ_DATA_12, RIRQ_DATA_13, RIRQ_DATA_14, RIRQ_DATA_15,
	RIRQ_DATA_16, RIRQ_DATA_17, RIRQ_DATA_18, RIRQ_DATA_19
};

void rirq_addr(RIRQCode * ic, byte n, void * addr)
{
	byte p = irqai[n];
	ic->code[p + 0] = (unsigned)addr & 0xff;
	ic->code[p + 1] = (unsigned)addr >> 8;
}

void rirq_data(RIRQCode * ic, byte n, byte data)
{
	byte p = irqdi[n];
	ic->code[p] = data;
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

void rirq_init(bool kernalIRQ)
{
	for(byte i=0; i<NUM_IRQS; i++)
	{
		rasterIRQRows[i] = 255;
		rasterIRQIndex[i] = i;
	}

    __asm 
    {
        sei

#if 0 
        // disable CIA interrupts

        lda #$7f
        sta $dc0d
        sta $dd0d
#endif
    }

    if (kernalIRQ)
    	*(void **)0x0314 = irq1;
    else
    	*(void **)0xfffe = irq0;

	vic.intr_enable = 1;
	vic.ctrl1 &= 0x7f;
	vic.raster = 255;
}

void rirq_wait(void)
{
	while (tpos != npos) ;
	npos++;
}

void rirq_sort(void)
{
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

	for(byte i=0; i<NUM_IRQS; i++)
		rasterIRQNext[i] = rasterIRQRows[rasterIRQIndex[i]];

	npos++;
	vic.raster = rasterIRQNext[nextIRQ] - 1;
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

