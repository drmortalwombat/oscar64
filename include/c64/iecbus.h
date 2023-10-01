#ifndef C64_IECBUS_H
#define C64_IECBUS_H

enum IEC_STATUS
{
	IEC_OK = 0x00,
	IEC_EOF = 0x01,

	IEC_ERROR = 0x80,
	IEC_TIMEOUT
};

extern IEC_STATUS iec_status;

bool iec_eoi(void);

bool iec_write(char b);

char iec_read(void);

void iec_atn(char dev, char sec);

void iec_talk(char dev, char sec);

void iec_untalk(void);

void iec_listen(char dev, char sec);

void iec_unlisten(void);

void iec_open(char dev, char sec, const char * fname);

void iec_close(char dev, char sec);

int iec_write_bytes(const char * data, int num);

int iec_read_bytes(char * data, int num);


#pragma compile("iecbus.c")

#endif

