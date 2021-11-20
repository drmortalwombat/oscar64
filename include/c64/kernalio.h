#ifndef C64_KERNALIO_H
#ifndef C64_KERNALIO_H


enum krnioerr
{
	KRNIO_OK = 0,
	KRNIO_DIR = 0x01,
	KRNIO_TIMEOUT = 0x02,
	KRNIO_SHORT = 0x04,
	KRNIO_LONG = 0x08,
	KRNIO_VERIFY = 0x10,
	KRNIO_CHKSUM = 0x20,
	KRNIO_EOF = 0x40,
	KRNIO_NODEVICE = 0x80
};

void krnio_setnam(const char * name);

bool krnio_open(char fnum, char device, char channel);

void krnio_close(char fnum);

krnioerr krnio_status(void);


bool krnio_chkout(char fnum);

bool krnio_chkin(char fnum);

void krnio_clrchn(void);

bool krnio_chrout(char ch);

int krnio_chrin(void);

int krnio_getch(char fnum);



int krnio_write(char fnum, const char * data, int num);

int krnio_puts(char fnum, const char * data);

int krnio_read(char fnum, char * data, int num);

int krnio_gets(char fnum, char * data, int num);

#pragma compile("kernalio.c")

#endif
