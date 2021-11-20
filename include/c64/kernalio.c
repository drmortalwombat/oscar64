#include "kernalio.h"


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

bool krnio_open(char fnum, char device, char channel)
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

void krnio_close(char fnum)
{
	__asm
	{
		lda	fnum
		jsr	$ffc3			// close
	}	
}

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

void krnio_clrchn(void)
{
	__asm
	{
		jsr $ffcc		// clrchn
	}
}

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

int krnio_chrin(void)
{
	__asm
	{
		jsr $ffcf		// chrin
		sta accu
		jsr $ffb7
		beq W1
		lda #$ff
		sta accu
	W1: 
		sta accu + 1
	}
}

int krnio_getch(char fnum)
{
	int	ch = -1;
	if (krnio_chkin(fnum))
		ch = krnio_chrin();
	krnio_clrchn();
	return ch;
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

int krnio_read(char fnum, char * data, int num)
{
	if (krnio_chkin(fnum))
	{
		int i = 0;
		int ch;
		while (i < num && (ch = krnio_chrin()) >= 0)
			data[i++] = (char)ch;
		krnio_clrchn();
		return i;
	}
	else
		return -1;	
}

int krnio_gets(char fnum, char * data, int num)
{
	if (krnio_chkin(fnum))
	{
		int i = 0;
		int ch;
		while (i + 1 < num && (ch = krnio_chrin()) >= 0)
		{
			data[i++] = (char)ch;
			if (ch == 13 || ch == 10)
				break;
		}
		data[i] = 0;
		krnio_clrchn();
		return i;
	}
	else
		return -1;	

}
