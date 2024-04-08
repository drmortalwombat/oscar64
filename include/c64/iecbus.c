#include "iecbus.h"
#include <c64/cia.h>
#include <c64/vic.h>

IEC_STATUS iec_status;

#define CIA2B_ATNOUT	0x08
#define CIA2B_CLKOUT	0x10
#define CIA2B_DATAOUT	0x20

#define CIA2B_CLKIN		0x40
#define CIA2B_DATAIN	0x80

#pragma optimize(push)
#pragma optimize(1)

// multiples of 5us
static void delay(char n)
{
	__asm {
		ldx n
	l1:
		dex
		bne	l1
	}
}

static inline void data_low(void)
{
	cia2.pra &= ~CIA2B_DATAOUT;
}

static inline void data_high(void)
{
	cia2.pra |= CIA2B_DATAOUT;
}

static inline void clock_low(void)
{
	cia2.pra &= ~CIA2B_CLKOUT;
}

static inline void cdata_low(void)
{
	cia2.pra &= ~(CIA2B_CLKOUT | CIA2B_DATAOUT);
}

static inline void clock_high(void)
{
	cia2.pra |= CIA2B_CLKOUT;
}

static inline void atn_low(void)
{
	cia2.pra &= ~CIA2B_ATNOUT;
}

static inline void atn_high(void)
{
	cia2.pra |= CIA2B_ATNOUT;
}

static bool data_check(void)
{
 	char cnt = 100;
	while (cnt > 0 && (cia2.pra & CIA2B_DATAIN))
		cnt--;

	if (cnt)
		return true;
	else
	{
		iec_status = IEC_DATA_CHECK;
		return false;
	}	
}

bool iec_eoi(void)
{
	cdata_low();

	while (!(cia2.pra & CIA2B_DATAIN))
		;
	delay(40);

	return data_check();
}

bool iec_write(char b)
{
	cdata_low();

	while (!(cia2.pra & CIA2B_DATAIN))
		;

	clock_high();

	for(char i=0; i<8; i++)
	{
		if (b & 1)
			data_low();
		else
			data_high();		
		delay(5);
		clock_low();
		b >>= 1;
		delay(5);
		clock_high();
		data_low();
	}

	return data_check();
}

char iec_read(void)
{
	while (!(cia2.pra & CIA2B_CLKIN))
		;

 	data_low();

 	char cnt = 100;
	while (cnt > 0 && (cia2.pra & CIA2B_CLKIN))
		cnt--;

	if (cnt == 0)
	{
		iec_status = IEC_EOF;
 		data_high();
		delay(4);
		data_low();

		cnt = 200;
		while (cnt > 0 && (cia2.pra & CIA2B_CLKIN))
			cnt--;

		if (cnt == 0)
		{
			iec_status = IEC_TIMEOUT;
			return 0;
		}
 	}

 	char b = 0;
 	for(char i=0; i<8; i++)
 	{
		char c;
		while (!((c = cia2.pra) & CIA2B_CLKIN))
			;

		b >>= 1;
		b |= c & 0x80;

		while (cia2.pra & CIA2B_CLKIN)
			;
 	}

 	data_high();
 	
 	return b;
}

void iec_atn(char dev, char sec)
{
	cdata_low();
	atn_high();
 	clock_high();

 	delay(200);

	iec_write(dev);
	if (sec != 0xff)
		iec_write(sec);

	data_high();
	atn_low();	
}


void iec_talk(char dev, char sec)
{
	iec_status = IEC_OK;

	iec_atn(dev | 0x40, sec | 0x60);
	clock_low();	

	delay(10);	
}

void iec_untalk(void)
{
	iec_atn(0x5f, 0xff);
}

void iec_listen(char dev, char sec)
{
	iec_status = IEC_OK;

	iec_atn(dev | 0x20, sec | 0x60);
}

void iec_unlisten(void)
{
	iec_atn(0x3f, 0xff);
}

void iec_open(char dev, char sec, const char * fname)
{
	iec_status = IEC_OK;

	iec_atn(dev | 0x20, sec | 0xf0);

	char i = 0;
	while (fname[i])
	{
		if (!fname[i + 1])
			iec_eoi();
		iec_write(fname[i]);
		i++;
	}
	iec_unlisten();
}

void iec_close(char dev, char sec)
{
	iec_atn(dev | 0x20, sec | 0xe0);
	iec_unlisten();
}

int iec_write_bytes(const char * data, int num)
{
	for(int i=0; i<num; i++)
	{
		if (!iec_write(data[i]))
			return i;
	}
	return num;
}

int iec_read_bytes(char * data, int num)
{
	int i = 0;
	while (i < num)
	{
		char ch = iec_read();
		if (iec_status < IEC_ERROR)
			data[i++] = ch;
		if (iec_status != IEC_OK)
			return i;
	}
	return num;
}


#pragma optimize(pop)

