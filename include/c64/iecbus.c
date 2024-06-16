#include "iecbus.h"
#include <c64/cia.h>
#include <c64/vic.h>

IEC_STATUS 	iec_status;
char		iec_queue;

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

static inline void data_true(void)
{
	cia2.pra &= ~CIA2B_DATAOUT;
}

static inline void data_false(void)
{
	cia2.pra |= CIA2B_DATAOUT;
}

static inline void clock_true(void)
{
	cia2.pra &= ~CIA2B_CLKOUT;
}

static inline void cdata_true(void)
{
	cia2.pra &= ~(CIA2B_CLKOUT | CIA2B_DATAOUT);
}

static inline void clock_false(void)
{
	cia2.pra |= CIA2B_CLKOUT;
}

static inline void atn_true(void)
{
	cia2.pra &= ~CIA2B_ATNOUT;
}

static inline void atn_false(void)
{
	cia2.pra |= CIA2B_ATNOUT;
}

static inline bool data_in(void)
{
	return (cia2.pra & CIA2B_DATAIN) != 0;
}

static inline bool clock_in(void)
{
	return (cia2.pra & CIA2B_CLKIN) != 0;
}

static bool data_check(void)
{
 	char cnt = 100;
	while (cnt > 0 && data_in())
		cnt--;

	if (cnt)
		return true;
	else
	{
		iec_status = IEC_DATA_CHECK;
		return false;
	}	
}

static bool iec_eoib(void)
{
	clock_true();

	while (!data_in());

	delay(40);

	return data_check();
}

static bool iec_writeb(char b)
{
	clock_true();

	while (!data_in());

	delay(5);
	for(char i=0; i<8; i++)
	{
		clock_false();
		delay(4);
		if (b & 1)
			data_true();
		else
			data_false();		
		clock_true();
		b >>= 1;
		delay(4);
	}
	clock_false();
	data_true();

	return data_check();
}

bool iec_write(char b)
{
	if (iec_status == IEC_QUEUED)
	{
		__asm
		{
			php
			sei
		}

		iec_status = IEC_OK;
		iec_writeb(iec_queue);

	 	__asm
		{
			plp
		}
	}
	if (iec_status < IEC_ERROR)
	{
		iec_queue = b;
		iec_status = IEC_QUEUED;
		return true;
	}

	return false;
}

char iec_read(void)
{
	while (!clock_in());

	__asm
	{
		php
		sei
	}

 	data_true();

 	char cnt = 100;
	while (cnt > 0 && clock_in())
		cnt--;

	if (cnt == 0)
	{
		iec_status = IEC_EOF;
 		data_false();
		delay(4);
		data_true();

		cnt = 200;
		while (cnt > 0 && clock_in())
			cnt--;

		if (cnt == 0)
		{
			iec_status = IEC_TIMEOUT;

		 	__asm
			{
				plp
			}
			
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

 	data_false();
 	
 	__asm
	{
		plp
	}

 	return b;
}

void iec_atn(char dev, char sec)
{	
	clock_true();
	data_true();
	atn_false();
	clock_false();

 	delay(200);

 	while (data_in());

	iec_writeb(dev);
	if (sec != 0xff)
		iec_writeb(sec);

	atn_true();	
}


void iec_talk(char dev, char sec)
{
	iec_status = IEC_OK;

	iec_atn(dev | 0x40, sec | 0x60);
	clock_true();
	data_false();

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
	__asm
	{
		php
		sei
	}

	if (iec_status == IEC_QUEUED)
	{
		iec_eoib();
		iec_writeb(iec_queue);
	}

	iec_atn(0x3f, 0xff);
	clock_true();

 	__asm
	{
		plp
	}
}

void iec_open(char dev, char sec, const char * fname)
{
	iec_status = IEC_OK;

	iec_atn(dev | 0x20, sec | 0xf0);

	char i = 0;
	while (fname[i])
	{
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

