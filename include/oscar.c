#include "oscar.h"

const char * oscar_expand_lzo(char * dp, const char * sp)
{
	char	cmd = sp[0];
	do
	{
		const 	char	*	cp;

		if (cmd & 0x80)
		{
			cp = dp - sp[1];
			cmd &= 0x7f;
			sp += 2;
		}
		else
		{
			sp += 1;
			cp = sp;
			sp += cmd;
		}

		char n = 0x00;
		do 	{
			dp[n] = cp[n];
			n++;
		} while (n != cmd);
		dp += cmd;

		cmd = sp[0];
	} while (cmd);

	return sp + 1;
}

const char * oscar_expand_rle(char * dp, const char * sp)
{
	char cmd = sp[0];

	do
	{
		if (cmd & 0x80)
		{
			char rep = (cmd & 0x70) >> 4;
			char	c = sp[1];
			for(signed char i=rep; i>=0; i--)
				dp[i] = c;

			rep++;
			sp += 2;
			dp += rep;

			cmd &= 0x0f;
			for(signed char i=cmd; i>=0; i--)
				dp[i] = sp[i];

			cmd++;
			sp += cmd;
			dp += cmd;
		}
		else if (cmd & 0x40)
		{
			cmd &= 0x3f;
			sp ++;
			for(signed char i=cmd; i>=0; i--)
				dp[i] = sp[i];

			cmd++;
			sp += cmd;
			dp += cmd;
		}
		else
		{
			char	c = sp[1];
			for(signed char i=cmd; i>=0; i--)
				dp[i] = c;

			cmd++;
			sp += 2;
			dp += cmd;
		}

		cmd = sp[0];

	} while (cmd);

	return sp + 1;
}

__native const char * oscar_expand_lzo_buf(char * dp, const char * sp)
{
	char	buf[256];
	char	b = 0;

	char	cmd = sp[0];
	do
	{
		if (cmd & 0x80)
		{
			char i = b - sp[1];
			cmd &= 0x7f;
			sp += 2;

			char n = 0;
			do 	{
				buf[b] = dp[n] = buf[i];
				b++;
				i++;
				n++;
			} while (n != cmd);
		}
		else
		{
			sp += 1;

			char n = 0;
			do 	{
				buf[b] = dp[n] = sp[n];
				b++;
				n++;
			} while (n != cmd);

			sp += cmd;
		}
		dp += cmd;

		cmd = sp[0];
	} while (cmd);

	return sp + 1;
}
