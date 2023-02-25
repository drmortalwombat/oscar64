#include "reu.h"

int reu_count_pages(void)
{
	volatile char	c, d;

	c = 0;
	reu_store(0, &c, 1);
	reu_load(0, &d, 1);

	if (d == 0)
	{
		c = 0x47;
		reu_store(0, &c, 1);
		reu_load(0, &d, 1);

		if (d == 0x47)
		{
			for(int i=1; i<256; i++)
			{
				long l = (long)i << 16;
				c = 0x47;
				reu_store(l, &c, 1);
				c = 0x00;
				reu_store(0, &c, 1);

				reu_load(l, &d, 1);
				if (d != 0x47)
					return i;
			}

			return 256;
		}
	}

	return 0;
}

inline void reu_store(unsigned long raddr, const volatile char * sp, unsigned length)
{
	reu.laddr = (word)sp;
	reu.raddr = raddr;
	reu.rbank = raddr >> 16;
	reu.length = length;
	reu.ctrl = REU_CTRL_INCL | REU_CTRL_INCR;
	reu.cmd = REU_CMD_EXEC | REU_CMD_FF00 | REU_CMD_STORE;

}

inline void reu_load(unsigned long raddr, volatile char * dp, unsigned length)
{
	reu.laddr = (word)dp;
	reu.raddr = raddr;
	reu.rbank = raddr >> 16;
	reu.length = length;
	reu.ctrl = REU_CTRL_INCL | REU_CTRL_INCR;
	reu.cmd = REU_CMD_EXEC | REU_CMD_FF00 | REU_CMD_LOAD;
}

inline void reu_fill(unsigned long raddr, char c, unsigned length)
{
	reu.laddr = (word)&c;
	reu.raddr = raddr;
	reu.rbank = raddr >> 16;
	reu.length = length;
	reu.ctrl = REU_CTRL_FIXL | REU_CTRL_INCR;
	reu.cmd = REU_CMD_EXEC | REU_CMD_FF00 | REU_CMD_STORE;
}

inline void reu_load2d(unsigned long raddr, volatile char * dp, char height, unsigned width, unsigned stride)
{
	reu.ctrl = REU_CTRL_INCL | REU_CTRL_INCR;
	reu.laddr = (word)dp;
	for(char i=0; i<height; i++)
	{
		reu.length = width;
		reu.raddr = raddr;
		reu.rbank = raddr >> 16;
		reu.cmd = REU_CMD_EXEC | REU_CMD_FF00 | REU_CMD_LOAD;
		raddr += stride;
	}	
}

inline void reu_load2dpage(unsigned long raddr, volatile char * dp, char height, unsigned width, unsigned stride)
{
	reu.ctrl = REU_CTRL_INCL | REU_CTRL_INCR;
	reu.laddr = (word)dp;
	reu.rbank = raddr >> 16;
	for(char i=0; i<height; i++)
	{
		reu.length = width;
		reu.raddr = raddr;
		reu.cmd = REU_CMD_EXEC | REU_CMD_FF00 | REU_CMD_LOAD;
		raddr += stride;
	}	
}

