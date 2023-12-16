#include "kernalio.h"

krnioerr krnio_pstatus[16];

void krnio_setnam(const char * name)
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
		jsr	$ffbd			// setnam
	}
}

#pragma native(krnio_setnam)

void krnio_setnam_n(const char * name, char len)
{
	__asm
	{
		lda len
		ldx name
		ldy	name + 1
		jsr	$ffbd			// setnam
	}	
}

#pragma native(krnio_setnam_n)

bool krnio_open(char fnum, char device, char channel)
{
	krnio_pstatus[fnum] = KRNIO_OK;

	__asm
	{
		lda	#0
		sta accu
		sta	accu + 1

		lda	fnum
		ldx	device
		ldy channel		
		jsr	$ffba			// setlfs
		
		jsr	$ffc0			// open
		bcc	W1

		lda	fnum
		jsr	$ffc3			// close
		jmp	E2
	W1:
		lda	#1
		sta	accu
	E2:
	}

}

#pragma native(krnio_open)

void krnio_close(char fnum)
{
	__asm
	{
		lda	fnum
		jsr	$ffc3			// close
	}	
}

#pragma native(krnio_close)

krnioerr krnio_status(void)
{
	__asm
	{
		jsr $ffb7			// readst
		sta accu
		lda #0
		sta accu + 1
	}
}

#pragma native(krnio_status)


bool krnio_load(char fnum, char device, char channel)
{
	__asm
	{
		lda	#0
		sta accu
		sta	accu + 1

		lda	fnum
		ldx	device
		ldy channel		
		jsr	$ffba			// setlfs
		
		lda #0
		ldx #0
		ldy #0
		jsr	$FFD5			// open
		bcc	W1

		lda #0
		jmp	E2
	W1:
		lda	#1
		sta	accu
	E2:
	}
}

#pragma native(krnio_load)

bool krnio_chkout(char fnum)
{
	__asm
	{
		ldx fnum
		jsr	$ffc9			// chkout
		lda #0
		sta accu + 1
		bcs W1
		lda #1
	W1: sta accu
	}
}

#pragma native(krnio_chkout)

bool krnio_chkin(char fnum)
{
	__asm
	{
		ldx fnum
		jsr	$ffc6			// chkin
		lda #0
		sta accu + 1
		bcs W1
		lda #1
	W1: sta accu
	}
}

#pragma native(krnio_chkin)

void krnio_clrchn(void)
{
	__asm
	{
		jsr $ffcc		// clrchn
	}
}

#pragma native(krnio_clrchn)

bool krnio_chrout(char ch)
{
		__asm
	{
		lda ch
		jsr $ffd2		// chrout
		sta accu
		lda #0
		sta accu + 1
	}
}

#pragma native(krnio_chrout)

int krnio_chrin(void)
{
	__asm
	{
		jsr $ffcf		// chrin
		sta accu
		lda #0
		sta accu + 1
	}
}

#pragma native(krnio_chrin)

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
		krnioerr err;
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
