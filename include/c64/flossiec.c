#include "flossiec.h"
#include <c64/iecbus.h>
#include <c64/vic.h>
#include <c64/cia.h>
#include <c64/kernalio.h>

#ifndef FLOSSIEC_NODISPLAY
#define FLOSSIEC_NODISPLAY	0
#endif
#ifndef FLOSSIEC_NOIRQ
#define FLOSSIEC_NOIRQ	0
#endif
#ifndef FLOSSIEC_BORDER
#define FLOSSIEC_BORDER	0
#endif


#define VIA_ATNIN	0x80
#define VIA_ATNOUT	0x10

#define VIA_CLKOUT	0x08
#define VIA_DATAOUT	0x02

#define VIA_CLKIN	0x04
#define VIA_DATAIN	0x01

#define PORTB1		0x1800
#define PORTB2		0x1c00

#define WR			0x1d

#ifdef FLOSSIEC_CODE
#pragma code(FLOSSIEC_CODE)
#endif
#ifdef FLOSSIEC_BSS
#pragma bss(FLOSSIEC_BSS)
#endif

__asm diskcode
{
	nop
	nop

	lda #VIA_CLKOUT
	sta PORTB1

	lda 0x0202
	sta 0x0c
	lda 0x0203
	sta 0x0d
	lda #$80
	sta 0x03

	ldx #0
l0:
	txa
	lsr
	lsr
	lsr
	lsr
	sta 0x0700,x
	inx
	bne l0

lr:
	lda 0x03
	bmi lr

	sei

	ldx #0
l2:
	lda #0
	sta PORTB1
	lda	0x0600, x
	tay
	and #0x0f
	ora #VIA_DATAIN
l1:
	bit PORTB1
	bne l1

l3:
	sta PORTB1

	tya
	asl
	and #0x0a
	sta PORTB1

	lda 0x0700,y
	nop
	sta PORTB1

	asl
	nop
	and #0x0a
	sta PORTB1

	inx
	bne	l2

	lda #VIA_CLKOUT
	sta PORTB1


	lda 0x0600
	beq w1
	sta 0x0c
	lda 0x0601
	sta 0x0d
	lda #$80
	sta 0x03
	cli
	bne lr
w1:
	sta PORTB1

	cli
	rts
}

#define CIA2B_ATNOUT	0x08
#define CIA2B_CLKOUT	0x10
#define CIA2B_DATAOUT	0x20

#define CIA2B_CLKIN		0x40
#define CIA2B_DATAIN	0x80

#define CIA2PRA			0xdd00

static char remap[256];
static char rbuffer[256];
static char xbuffer[256];
static char flpos;
static char xcmd;
static char xi, xj;
static char fldrive;
static char flvxor;

__noinline void fl_read_buf(void)
{
	__asm
	{
#if FLOSSIEC_NOIRQ
		php
		sei
#endif

		lda CIA2PRA
		and #~CIA2B_CLKOUT
		sta accu
		sta CIA2PRA
		and #~CIA2B_DATAOUT
		sta accu + 1

	l0:
		lda CIA2PRA
		and #CIA2B_CLKIN
		beq l0
#if !FLOSSIEC_NOIRQ
		php
		pla
		and #$04
		beq iq
#endif
		ldy #0
		sec
	l1:
		ldx accu + 1
#if !FLOSSIEC_NODISPLAY
	l2:
		lda 0xd012
		sbc #50
		bcc w1
		and #7
		beq l2
#endif
	w1:
		stx CIA2PRA

#if FLOSSIEC_BORDER
		inc 0xd020
#else
		nop
		nop
		nop
#endif
		ldx accu
		nop

		lda CIA2PRA
		lsr
		lsr
		nop
		eor CIA2PRA
		lsr
		lsr
		nop
		eor CIA2PRA
		lsr
		lsr
		sec
		eor CIA2PRA
		stx CIA2PRA

		sta rbuffer, y
		iny
		bne l1
		jmp done
#if !FLOSSIEC_NOIRQ
	iq:
		ldy #0
		sec
	l1i:
		ldx accu + 1		
	l2i:
		cli
		sei
#if !FLOSSIEC_NODISPLAY
		lda 0xd012
		sbc #50
		bcc w1i
		and #7
		beq l2i
	w1i:
#endif
		stx CIA2PRA

#if FLOSSIEC_BORDER
		inc 0xd020
#else
		nop
		nop
		nop
#endif
		ldx accu
		nop

		lda CIA2PRA
		lsr
		lsr
		nop
		eor CIA2PRA
		lsr
		lsr
		nop
		eor CIA2PRA
		lsr
		lsr
		sec
		eor CIA2PRA
		stx CIA2PRA

		sta rbuffer, y
		iny
		bne l1i
		cli
#endif
	done:	

#if FLOSSIEC_NOIRQ
		plp		
#endif
	}
}



inline char flossiec_get(void)
{
	if (!flpos)
	{
		fl_read_buf();
		flpos = 2;
	}
	return remap[rbuffer[flpos++]];
}

void flossiec_decompress(void)
{
	char i  = 0, j = xj, cmd = xcmd;
	xi = 0;

	for(;;)
	{
		if (cmd & 0x80)
		{
			if (i < cmd)
			{
				char t = i - cmd;
				do {
					char ch = xbuffer[j++];
					xbuffer[i++] = ch;
				} while (i != t);
				cmd = 0;
			}
			else
			{
				cmd -= i;
				do {
					char ch = xbuffer[j++];
					xbuffer[i++] = ch;
				} while (i);
				break;
			}
		}
		else 
		{
			char ch = flossiec_get();
			if (cmd)
			{
				xbuffer[i++] = ch;
				cmd--;
				if (!i)
					break;
			}
			else
			{
				cmd = ch;
				if (!cmd)
					break;
				if (cmd & 0x80)
				{
					cmd ^= 0x7f;
					cmd++;
					j = i - flossiec_get();
				}
			}
		}
	}
	xj = j;
	xcmd = cmd;	
}

inline char flossiec_get_lzo(void)
{
	if (!xi)
		flossiec_decompress();
	return xbuffer[xi++];
}

inline bool flossiec_eof(void)
{
	return !remap[rbuffer[0]] && flpos >= remap[rbuffer[1]];
}

char * flossiec_read(char * dp, unsigned size)
{
	while (size)
	{
		*dp++ = flossiec_get();
		size--;
	}

	return dp;
}

char * flossiec_read_lzo(char * dp, unsigned size)
{
	char i = xi;

	dp -= i;
	size += i;

	while (size)
	{
		if (!i)
			flossiec_decompress();
		
		if (size >= 256)
		{
			do {
				dp[i] = xbuffer[i];
				i++;
			} while (i);
			dp += 256;
			size -= 256;
		}
		else
		{
			do {
				dp[i] = xbuffer[i];
				i++;
			} while (i != (char)size);
			dp += i;
			break;
		}
	}

	xi = i;

	return dp;
}

static void vxorcheck(void)
{
	char vxor = cia2.pra & 7;
	vxor ^= vxor >> 2;
	vxor ^= 0xff;

	if (vxor != flvxor)
	{
		flvxor = vxor;

		for(int i=0; i<256; i++)
		{
			char j = i ^ vxor;
			char d = ((j & 0x11) << 3) |
			          (j & 0x66) |
			         ((j & 0x88) >> 3);
			remap[i] = d;
		}
	}
}

bool flossiec_init(char drive)
{
	fldrive = drive;
	flvxor = 0;

	iec_open(drive, 2, "#2");
	iec_listen(drive, 2);
	for(char j=0; j<127; j++)
		iec_write(((char *)diskcode)[j]);
	iec_unlisten();		

	iec_close(drive, 2);

	iec_open(drive, 15, "");

	return true;	
}

void flossiec_shutdown(void)
{
	iec_close(fldrive, 15);
}


bool flossiec_open(char track, char sector)
{
	iec_listen(fldrive, 15);
	iec_write(P'U');
	iec_write(P'4');
	iec_write(track);
	iec_write(sector);
	iec_unlisten();	

	cia2.pra |= CIA2B_DATAOUT;


#if FLOSSIEC_NODISPLAY
	vic.ctrl1 &= ~VIC_CTRL1_DEN;
#endif

	vic_waitFrame();
	vxorcheck();
	vic_waitFrame();

	flpos = 0;
	xi = 0;

	return true;
}

void flossiec_close(void)
{
	cia2.pra |= CIA2B_DATAOUT;

#if FLOSSIEC_NODISPLAY
	vic.ctrl1 |= VIC_CTRL1_DEN;
#endif	
}

bool flosskio_init(char drive)
{
	fldrive = drive;
	flvxor = 0;

	krnio_setnam_n("#2", 2);
	krnio_open(2, drive, 2);
	krnio_write(2, (char *)diskcode, 128);
	krnio_close(2);

	krnio_setnam_n(nullptr, 0);
	krnio_open(15, drive, 15);

	return true;	
}

void flosskio_shutdown(void)
{
	krnio_close(15);
}


bool flosskio_open(char track, char sector)
{
	krnio_chkout(15);
	krnio_chrout(P'U');
	krnio_chrout(P'4');
	krnio_chrout(track);
	krnio_chrout(sector);
	krnio_clrchn();

	cia2.pra |= CIA2B_DATAOUT;

#if FLOSSIEC_NODISPLAY
	vic.ctrl1 &= ~VIC_CTRL1_DEN;
#endif

	vic_waitFrame();
	vxorcheck();
	vic_waitFrame();

	flpos = 0;
	xi = 0;

	return true;
}

void flosskio_close(void)
{
	cia2.pra |= CIA2B_DATAOUT;

#if FLOSSIEC_NODISPLAY
	vic.ctrl1 |= VIC_CTRL1_DEN;
#endif	
}

static bool mapdir(const char * fnames, floss_blk * blks)
{
	do {
		fl_read_buf();

		char si = 0;
		do
		{
			if (remap[rbuffer[si + 2]] == 0x82)
			{
				char fname[17];
				char j = 0;
				while (j < 16 && remap[rbuffer[si + j + 5]] != 0xa0)
				{
					fname[j] = remap[rbuffer[si + j + 5]];
					j++;
				}
				fname[j] = 0;

				char sj = 0;
				char k = 0;
				while (fnames[sj])
				{
					j = 0;
					while (fname[j] && fname[j] == fnames[sj])
					{
						j++;
						sj++;
					}
					if (!fname[j] && (!fnames[sj] || fnames[sj] == ','))
					{
						__assume(k < 128);
						blks[k].track = remap[rbuffer[si + 3]];
						blks[k].sector = remap[rbuffer[si + 4]];
						break;
					}

					while (fnames[sj] && fnames[sj++] != ',')
						;
					k++;
				}
			}

			si += 32;
		} while (si);

	}	while (remap[rbuffer[0]]);

	return true;
}

bool flosskio_mapdir(const char * fnames, floss_blk * blks)
{
	if (flosskio_open(18, 1))
	{
		mapdir(fnames, blks);

		flosskio_close();
		return true;
	}

	return false;
}

bool flossiec_mapdir(const char * fnames, floss_blk * blks)
{
	if (flossiec_open(18, 1))
	{
		mapdir(fnames, blks);

		flossiec_close();
		return true;
	}

	return false;
}

