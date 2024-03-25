
#ifndef C64_KERNALIO_H

// Error and status codes returned by krnio_status

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

extern krnioerr krnio_pstatus[16];

#if defined(__C128__) || defined(__C128B__) || defined(__C128E__)
// C128: Set bank for load/save and filename for next file operations
void krnio_setbnk(char filebank, char namebank);
#endif

// Set filename for next krnio_open operation, make sure
// that the string is still valid when calling krnio_open

void krnio_setnam(const char * name);

void krnio_setnam_n(const char * name, char len);

// open a kernal file/stream/io channel, returns true on success

bool krnio_open(char fnum, char device, char channel);

// close a kernal file/stream/io channel

void krnio_close(char fnum);

// get the error / status of the last io operation

krnioerr krnio_status(void);

bool krnio_load(char fnum, char device, char channel);

bool krnio_save(char device, const char* start, const char* end);

// select the given file for stream output

bool krnio_chkout(char fnum);

// select the given file for stream input

bool krnio_chkin(char fnum);

// clear input and output file selection

void krnio_clrchn(void);

// write a single byte to the current output channel

bool krnio_chrout(char ch);

// read a single byte from the current input channel

int krnio_chrin(void);

// read a single byte from the given file/channel, returns
// a negative result on failure.  If this was the last byte
// the bit #8 (0x0100) will be set in the return value

int krnio_getch(char fnum);

// write a single byte to the given file/channel, returns
// a negative value on failure.

int krnio_putch(char fnum, char ch);

// write an array of bytes to the given file/channel

int krnio_write(char fnum, const char * data, int num);

// write a zero terminated string to the given file/channel

int krnio_puts(char fnum, const char * data);

// read an array of bytes from the given file, returns the number
// of bytes read, or a negative number on failure

int krnio_read(char fnum, char * data, int num);

int krnio_read_lzo(char fnum, char * data);

// read a line from the given file, terminated by a CR or LF character
// and appends a zero byte.

int krnio_gets(char fnum, char * data, int num);

#pragma compile("kernalio.c")

#endif
