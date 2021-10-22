#include "rasterirq.h"

#include <c64/vic.h>
#include <c64/cia.h>

volatile char npos = 1, tpos = 0;

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
	sbc	#4
	cmp	$d012
	bcc l1

	dey
	sty	$d012
w1:
	jmp ex

e2:
	sta $d012

	lda npos
	sta tpos

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
	sbc	#4
	cmp	$d012
	bcc l1

	dey
	sty	$d012
w1:
	jmp ex

e2:
	sta $d012

	lda npos
	sta tpos

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

	ic->code[0] = 0xa9;	// lda #
	ic->code[2] = 0xa0; // ldy #
	ic->code[4] = 0xec; // cpx
	ic->code[5] = 0x12;
	ic->code[6] = 0xd0; 
	ic->code[7] = 0xb0; // bcs
	ic->code[8] = -5;
	ic->code[9] = 0x8d; // sta

	if (size == 1)
	{
		ic->code[12] = 0x60; // rts
	}
	else
	{
		ic->code[12] = 0x8c; // sty

		byte p = 15;
		for(byte i=2; i<size; i++)
		{
			ic->code[p] = 0xa9; // lda #
			ic->code[p + 2] = 0x8d; // sta
			p += 5;
		}
		ic->code[p] = 0x60;
	}
}

void rirq_set(byte n, byte row, RIRQCode * write)
{
	rasterIRQLow[n] = (unsigned)&write->code & 0xff;
	rasterIRQHigh[n] = (unsigned)&write->code >> 8;

	rasterIRQRows[n] = row;
}

static const byte irqai[6] = {RIRQ_ADDR_0, RIRQ_ADDR_1, RIRQ_ADDR_2, RIRQ_ADDR_3, RIRQ_ADDR_4};
static const byte irqdi[6] = {RIRQ_DATA_0, RIRQ_DATA_1, RIRQ_DATA_2, RIRQ_DATA_3, RIRQ_DATA_4};

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

        // disable CIA interrupts

        lda #$7f
        sta $dc0d
        sta $dd0d

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
}

void rirq_sort(void)
{
	for(byte i = 1; i<NUM_IRQS; i++)
	{
		byte ri = rasterIRQIndex[i];
		byte rr = rasterIRQRows[ri];
		byte j = i, rj = rasterIRQIndex[j - 1];
		while (j > 0 && rr < rasterIRQRows[rj])
		{
			rasterIRQIndex[j] = rj;
			j--;
			rj = rasterIRQIndex[j - 1] 
		}
		rasterIRQIndex[j] = ri;
	}

	for(byte i=0; i<NUM_IRQS; i++)
		rasterIRQNext[i] = rasterIRQRows[rasterIRQIndex[i]];

	npos++;
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
