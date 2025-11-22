#include "kernalio.h"

krnioerr krnio_pstatus[16];

#if defined(__C128__) || defined(__C128B__) || defined(__C128E__)
void krnio_setbnk(char filebank, char namebank)
{
	__asm
	{
		lda filebank
		ldx namebank
		jsr $ff68			// setbnk
	}
}

#pragma native(krnio_setbnk)
#endif

#if defined(__PLUS4__)
#pragma code(lowcode)
#define BANKIN	sta 0xff3e
#define BANKOUT	sta 0xff3f
#define BANKINLINE	__noinline
#else
#define BANKIN
#define BANKOUT
#define BANKINLINE
#endif

#if defined(__CBMPET__)
#define FNLEN		0xD1 // Length of filename
#define LFN		0xD2 // Current Logical File Number
#define SECADR		0xD3 // Secondary address
#define DEVNUM		0xD4 // Device number
#define FNADR		0xDA // Pointer to file name
#define ST		0x90 // IEC status byte

// PET ROM Detection
#define PET_DETECT	0xFFFB	// Distinction V2 vs V4 BASIC
#define PET_2000	0xCA
#define PET_3000	0xFC
#define PET_4000	0xFD

inline void k_checkst()
{
	__asm
	{
		lda	ST
		beq	l1
		lda	#5	// Device not present
		sec
		rts
	l1:
		clc
	};
}

#pragma native(k_checkst)

inline void k_setlfs()
{
	__asm
	{
		sta	LFN			// setlfs replacement
		stx	DEVNUM
		sty	SECADR
	};
}

#pragma native(k_setlfs)

inline void k_open()
{
	__asm
	{
		lda PET_DETECT
		cmp #PET_4000
		bne	V2
		jsr	$f563
		jsr	k_checkst
		rts
	V2:
		jsr	$f524
		jsr	k_checkst
	};
}

#pragma native(k_open)

inline void k_close()
{
	__asm
	{
		ldx	PET_DETECT
		cpx	#PET_4000
		bne	l1
		jmp	$F2E2			// BASIC 4
	l1:
		jmp	$F2AE			//BASIC 2&3
	};
}

#pragma native(k_open)

#endif

BANKINLINE void krnio_setnam(const char * name)
{
	__asm
	{
		lda name
		ora name + 1
		beq W1

		ldy	#$ff
	L1:	iny
		lda	(name), y
		bne	L1
		tya
	W1: ldx name
		ldy	name + 1
		BANKIN
#if defined(__CBMPET__)
		sta	FNLEN
		stx	FNADR
		sty	FNADR+1
#else
		jsr	$ffbd			// setnam
#endif
		BANKOUT
	}
}

#pragma native(krnio_setnam)

BANKINLINE void krnio_setnam_n(const char * name, char len)
{
	__asm
	{
		lda len
		ldx name
		ldy	name + 1
		BANKIN
#if defined(__CBMPET__)
		sta	FNLEN
		stx	FNADR
		sty	FNADR+1
#else
		jsr	$ffbd			// setnam
#endif
		BANKOUT
	}
}

#pragma native(krnio_setnam_n)

BANKINLINE bool krnio_open(char fnum, char device, char channel)
{
	krnio_pstatus[fnum] = KRNIO_OK;

	return char(__asm
	{
		lda	#0
		sta	accu
		sta	accu + 1

		BANKIN

		lda	fnum
		ldx	device
		ldy	channel
#if defined(__CBMPET__)
		jsr	k_setlfs
		
		jsr	k_open
#else
		jsr	$ffba			// setlfs

		jsr	$ffc0			// open
#endif
	W3:
		bcc	W1

		lda	fnum
#if defined(__CBMPET__)
		jsr	k_close
#else
		jsr	$ffc3			// close
#endif
		jmp	E2
	W1:
		lda	#1
		sta	accu

		BANKOUT
	E2:
	});
}

#pragma native(krnio_open)

BANKINLINE void krnio_close(char fnum)
{
	__asm
	{
		BANKIN
		lda	fnum
#if defined(__CBMPET__)
		jsr	k_close
#else
		jsr	$ffc3			// close
#endif
		BANKOUT
	}
}

#pragma native(krnio_close)

BANKINLINE krnioerr krnio_status(void)
{
	return __asm
	{
		BANKIN
		jsr $ffb7	: ->a		// readst
		BANKOUT
		sta accu
		lda #0
		sta accu + 1
	};
}

#pragma native(krnio_status)


BANKINLINE bool krnio_load(char fnum, char device, char channel)
{
	return char(__asm
	{
		BANKIN
		lda	fnum
		ldx	device
		ldy channel		
		jsr	$ffba			// setlfs
		
		lda #0
		ldx #0
		ldy #0
		jsr	$FFD5			// load
		BANKOUT

		lda #0
		rol
		eor #1
		sta accu
	});
}

#pragma native(krnio_load)

BANKINLINE bool krnio_save(char device, const char* start, const char* end)
{
	return char(__asm
	{
		BANKIN
		lda	#0
		ldx	device
		ldy	#0
		jsr	$ffba			// setlfs
		
		lda #start
		ldx end
		ldy end+1
		jsr	$FFD8			// save

		BANKOUT

		lda #0
		rol
		eor #1
		sta accu
	});
}

#pragma native(krnio_save)

BANKINLINE bool krnio_chkout(char fnum)
{
	return char(__asm
	{
		BANKIN
		ldx fnum
		jsr	$ffc9	: x->ax		// chkout
		BANKOUT

		lda #0
		rol
		eor #1
		sta accu
	});
}

#pragma native(krnio_chkout)

BANKINLINE bool krnio_chkin(char fnum)
{
	return char(__asm
	{
		BANKIN
		ldx fnum
		jsr	$ffc6	: x->axy	// chkin
		BANKOUT

		lda #0
		rol
		eor #1
		sta accu
	});
}

#pragma native(krnio_chkin)

BANKINLINE void krnio_clrchn(void)
{
	__asm
	{
		BANKIN
		jsr $ffcc	: ->ax	// clrchn
		BANKOUT
	}
}

#pragma native(krnio_clrchn)

BANKINLINE bool krnio_chrout(char ch)
{
	return char(__asm
	{
		BANKIN
		lda ch
		jsr $ffd2	: a->a	// chrout
		sta accu
		BANKOUT
	});
}

#pragma native(krnio_chrout)

BANKINLINE char krnio_chrin(void)
{
	return __asm
	{
		BANKIN
		jsr $ffcf	: a->a	// chrin
		sta accu
		BANKOUT
	};
}

#pragma native(krnio_chrin)

#if defined(__PLUS4__)
#pragma code(code)
#endif

int krnio_getch(char fnum)
{
	if (krnio_pstatus[fnum] == KRNIO_EOF)
		return -1;

	int	ch = -1;
	if (krnio_chkin(fnum))
	{
		ch = krnio_chrin();
		krnioerr err = krnio_status();
		krnio_pstatus[fnum] = err;
		if (err)
		{	
			if (err == KRNIO_EOF)
				ch |= 0x100;
			else
				ch = -1;
		}
	}
	krnio_clrchn();
	return ch;
}

int krnio_putch(char fnum, char ch)
{
	if (krnio_chkout(fnum))
	{
		krnio_chrout(ch);
		krnio_clrchn();
		return 0;
	}
	else
		return -1;	
}


int krnio_puts(char fnum, const char * data)
{
	if (krnio_chkout(fnum))
	{
		int i = 0;
		while (data[i])
			krnio_chrout(data[i++]);
		krnio_clrchn();
		return i;
	}
	else
		return -1;
}

#pragma native(krnio_puts)

int krnio_write(char fnum, const char * data, int num)
{
	if (krnio_chkout(fnum))
	{
		for(int i=0; i<num; i++)
			krnio_chrout(data[i]);
		krnio_clrchn();
		return num;
	}
	else
		return -1;
}

#pragma native(krnio_write)

int krnio_read(char fnum, char * data, int num)
{
	if (krnio_pstatus[fnum] == KRNIO_EOF)
		return 0;

	if (krnio_chkin(fnum))
	{
		int i = 0;
		int ch;
		while (i < num)
		{
			ch = krnio_chrin();
			krnioerr err = krnio_status();
			krnio_pstatus[fnum] = err;
			if (err && err != KRNIO_EOF)
				break;
			data[i++] = (char)ch;
			if (err)
				break;
		}
		krnio_clrchn();
		return i;
	}
	else
		return -1;	
}

#pragma native(krnio_read)

int krnio_read_lzo(char fnum, char * data)
{
	if (krnio_pstatus[fnum] == KRNIO_EOF)
		return 0;

	if (krnio_chkin(fnum))
	{
		int i = 0;
		char ch;
		char cmd = 0;
		krnioerr err;

		for(;;)
		{
			ch = krnio_chrin();
			err = krnio_status();
			if (err && err != KRNIO_EOF)
				break;

			if (cmd & 0x80)				
			{

				char * dp = data + i, * cp = dp - ch;

				cmd &= 0x7f;
				i += cmd;

				char n = 0x00;
				do 	{
					dp[n] = cp[n];
					n++;
				} while (n != cmd);
				cmd = 0;
			}
			else if (cmd)
			{
				data[i++] = (char)ch;
				cmd--;
			}
			else if (ch)
				cmd = ch;
			else
				break;

			if (err)
				break;

		} 

		krnio_pstatus[fnum] = err;

		krnio_clrchn();
		return i;
	}
	else
		return -1;	
}

#pragma native(krnio_read_lzo)

int krnio_gets(char fnum, char * data, int num)
{
	if (krnio_pstatus[fnum] == KRNIO_EOF)
		return 0;

	if (krnio_chkin(fnum))
	{
		krnioerr err = KRNIO_OK;
		int i = 0;
		int ch;
		while (i + 1 < num)
		{
			ch = krnio_chrin();
			err = krnio_status();
			if (err && err != KRNIO_EOF)
				break;
			data[i++] = (char)ch;
			if (ch == 13 || ch == 10 || err)
				break;
		}
		krnio_pstatus[fnum] = err;
		data[i] = 0;
		krnio_clrchn();
		return i;
	}
	else
		return -1;	

}

#pragma native(krnio_gets)
