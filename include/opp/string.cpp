#include "string.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

namespace opp {

static inline void smemcpy(char * dp, const char * sp, char s)
{
	for(char i=0; i<s; i++)
		dp[i] = sp[i];
}

static inline char sstrlen(const char * sp)
{
	char n = 0;
	while (sp[n])
		n++;
	return n;
}

inline void swap(string & u, string & v)
{
	char * p = u.cstr; u.cstr = v.cstr; v.cstr = p;
}

string::string(void) : cstr(nullptr)
{}

string::string(const string & s)
{
	if (s.cstr)
	{
		char l = s.cstr[0];
		cstr = malloc(char(l + 2));
		smemcpy(cstr, s.cstr, l + 2);
	}
	else
		cstr = nullptr;
}

string::string(string && s)
	: cstr(s.cstr)
{
	s.cstr = nullptr;
}

string::string(const char * s)
{
	if (s)
	{	
		char l = sstrlen(s);
		if (l)
		{
			cstr = malloc(char(l + 2));
			cstr[0] = l;
			smemcpy(cstr + 1, s, l + 1);
		}
		else
			cstr = nullptr;
	}
	else
		cstr = nullptr;
}

string::string(const char * s, char size)
{
	if (size)
	{
		cstr = malloc(char(size + 2));
		cstr[0] = size;
		smemcpy(cstr + 1, s, size + 1);
	}
	else
		cstr = nullptr;	
}

string::string(char c)
{
	cstr = malloc(3);
	cstr[0] = 1;
	cstr[1] = c;
	cstr[2] = 0;
}

string::string(char l, char * b)
	: cstr(b)
{
	b[0] = l;
	b[l + 1] = 0;
}

string::~string(void)
{
	free(cstr);
}

void string::clear(void)
{
	free(cstr);
	cstr = nullptr;	
}
	
void string::copyseg(char * p, char at, char num) const
{
	smemcpy(p, cstr + 1 + at, num);
}

string & string::operator=(const string & s)
{
	if (cstr != s.cstr)	
	{
		free(cstr);
		if (s.cstr)
		{
			char l = s.cstr[0];
			cstr = malloc(char(l + 2));
			smemcpy(cstr, s.cstr, l + 2);
		}
		else
			cstr = nullptr;
	}

	return *this;
}

string & string::operator=(string && s)
{
	if (cstr != s.cstr)	
	{
		free(cstr);
		cstr = s.cstr;
		s.cstr = nullptr;
	}

	return *this;
}

string & string::operator=(const char * s)
{
	free(cstr);
	if (s)
	{
		char l = sstrlen(s);
		if (l)
		{
			cstr = malloc(char(l + 2));
			cstr[0] = l;		
			smemcpy(cstr + 1, s, l + 1);
		}
		else
			cstr = nullptr;
	}
	else
		cstr = nullptr;

	return *this;	
}

string & string::operator+=(const string & s)
{
	if (s.cstr)
	{
		char d = 0;
		if (cstr)
			d = cstr[0];

		char l = s.cstr[0] + d;
		char * c = malloc(char(l + 2));
		c[0] = l;

		if (d)
			smemcpy(c + 1, cstr + 1, d);
		smemcpy(c + 1 + d, s.cstr + 1, s.cstr[0] + 1);
		free(cstr);
		cstr = c;
	}
	return *this;
}

string & string::operator+=(const char * s)
{
	if (s)
	{
		char sl = sstrlen(s);
		if (sl)
		{
			if (cstr)
			{
				char l = sl + cstr[0];
				char * c = malloc(char(l + 2));
				c[0] = l;
				smemcpy(c + 1, cstr + 1, cstr[0]);
				smemcpy(c + 1 + cstr[0], s, sl + 1);
				free(cstr);
				cstr = c;
			}
			else
			{
				cstr = malloc(char(sl + 2));
				cstr[0] = sl;
				smemcpy(cstr + 1, s, sl + 1);
			}
		}
	}
	return *this;
}

string & string::operator+=(char c)
{
	if (cstr)
	{
		char l = cstr[0] + 1;
		char * p = malloc(char(l + 2));
		p[0] = l;
		smemcpy(p + 1, cstr + 1, cstr[0]);
		p[l] = c;
		p[l + 1] = 0;
		free(cstr);
		cstr = p;
	}
	else
	{
		cstr = malloc(3);
		cstr[0] = 1;
		cstr[1] = c;
		cstr[2] = 0;
	}	
	return *this;
}


inline const char * string::tocstr(void) const
{
	if (cstr)
		return cstr + 1;
	else
		return "";
}

inline unsigned string::size(void) const
{
	if (cstr)
		return cstr[0];
	else
		return 0;
}

string string::operator+(const string & s) const
{
	if (cstr)
	{
		if (s.cstr)
		{
			char l = cstr[0] + s.cstr[0];
			char * p = malloc(char(l + 2));
			smemcpy(p + 1, cstr + 1, cstr[0]);
			smemcpy(p + 1 + cstr[0], s.cstr + 1, s.cstr[0]);
			return string(l, p);
		}
		else
			return *this;
	}
	else
		return s;
}

string string::operator+(const char * s) const
{
	if (cstr)
	{
		if (s)
		{
			char sl = sstrlen(s);
			if (sl)
			{
				char l = cstr[0] + sl;
				char * p = malloc(char(l + 2));
				smemcpy(p + 1, cstr + 1, cstr[0]);
				smemcpy(p + 1 + cstr[0], s, sl);
				return string(l, p);
			}
			else
				return *this;
		}
		else
			return *this;
	}
	else
		return string(s);
}

string string::operator+(char c) const
{
	if (cstr)
	{
		char l = cstr[0] + 1;
		char * p = malloc(char(l + 2));
		smemcpy(p + 1, cstr + 1, cstr[0]);
		p[l] = c;
		return string(l, p);
	}
	else
		return string(c);
}

string & string::operator<<=(char n)
{
	if (n > 0 && cstr)
	{
		if (n >= cstr[0])
		{
			free(cstr);
			cstr = nullptr;
		}
		else
		{
			char i = 1;
			n++;
			while (cstr[n])
				cstr[i++] = cstr[n++];
			cstr[i] = 0;
			cstr[0] = i - 1;
		}
	}
	return *this;
}

string & string::operator>>=(char n)
{
	if (n > 0 && cstr)
	{
		if (n >= cstr[0])
		{
			free(cstr);
			cstr = nullptr;
		}
		else
		{
			cstr[0] -= n;
			cstr[n + 1] = 0;
		}
	}
	return *this;
}

string string::operator<<(char n) const
{
	if (n > 0 && cstr)
	{
		if (n >= cstr[0])
			return string();
		else
		{

		}
	}
	else
		return *this;
}

string string::operator>>(char n) const
{

}

static int scmp(const char * s1, const char * s2)
{
	char n = 0;
	while (s1[n])
	{
		if (s1[n] != s2[n])
			return s1[n] - s2[n];
		n++;
	}
	return 0;
}

inline bool string::operator==(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) == 0;
	else
		return !cstr && !s.cstr;
}

inline bool string::operator==(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) == 0;
	else
		return s[0] == 0;
}

inline bool string::operator!=(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) != 0;
	else
		return cstr || s.cstr;
}

inline bool string::operator!=(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) == 0;
	else
		return s[0] != 0;
}

inline bool string::operator<(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) < 0;
	else
		return s.cstr;
}

inline bool string::operator<(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) < 0;
	else
		return s[0] != 0;
}

inline bool string::operator<=(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) <= 0;
	else
		return !cstr || s.cstr;
}

inline bool string::operator<=(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) <= 0;
	else
		return true;
}

inline bool string::operator>(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) > 0;
	else
		return cstr;
}

inline bool string::operator>(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) > 0;
	else
		return false;
}

inline bool string::operator>=(const string & s) const
{
	if (cstr && s.cstr)
		return scmp(cstr + 1, s.cstr + 1) >= 0;
	else
		return cstr || !s.cstr;
}

inline bool string::operator>=(const char * s) const
{
	if (cstr)
		return scmp(cstr + 1, s) >= 0;
	else
		return s[0] == 0;
}

inline char & string::operator[](char t)
{
	return cstr[t + 1];
}

inline char string::operator[](char t) const
{
	return cstr[t + 1];
}

char * string::begin(void)
{
	return cstr ? cstr + 1 : nullptr;
}

const char * string::begin(void) const
{
	return cstr ? cstr + 1 : nullptr;
}

const char * string::cbegin(void) const
{
	return cstr ? cstr + 1 : nullptr;
}

char * string::end(void)
{
	return cstr ? cstr + 1 + cstr[0] : nullptr;
}

const char * string::end(void) const
{
	return cstr ? cstr + 1 + cstr[0] : nullptr;
}

const char * string::cend(void) const
{
	return cstr ? cstr + 1 + cstr[0] : nullptr;
}


string string::substr(char pos, char len) const
{
	if (!cstr || len == 0 || pos >= cstr[0])
		return string();
	else
	{
		char l = cstr[0];
		if (pos + len > l)
			len = l - pos;

		char * p = malloc(len + 2);
		memcpy(p + 1, cstr + 1 + pos, len);
		return string(len, p);
	}
}

inline int string::find(const string & s) const
{
	return find(s, 0);
}

inline int string::find(const char * s) const
{
	return find(s, 0);
}

inline int string::find(char c) const
{
	return find(c, 0);
}

int string::find(const string & s, char pos) const
{
	if (!s.cstr)
		return pos;
	if (cstr)
	{
		char l = cstr[0];
		char sl = s.cstr[0];
		if (sl <= l)
		{
			l -= sl;

			while (pos <= l)
			{
				char i = 1;
				while (s.cstr[i] && s.cstr[i] == cstr[pos + i])
					i++;
				if (!s.cstr[i])
					return pos;
				pos++;
			}
		}
	}

	return -1;
}

int string::find(const char * s, char pos) const
{
	if (cstr)
	{
		char l = cstr[0];
		char sl = sstrlen(s);
		if (sl <= l)
		{
			l -= sl;

			while (pos <= l)
			{
				char i = 0;
				while (s[i] && s[i] == cstr[pos + i + 1])
					i++;
				if (!s[i])
					return pos;
				pos++;
			}
		}
	}

	return -1;
}

int string::find(char c, char pos) const
{
	if (cstr)
	{
		char l = cstr[0];
		while (pos < l)
		{
			if (cstr[pos + 1] == c)
				return pos;
			pos++;
		}		
	}

	return -1;
}

string to_string(int val)
{
	char	buffer[10];

	bool sign = false;
	if (val < 0)
	{
		val = -val;
		sign = true;
	}

	char i = 10;

	while (val)
	{
		char d = val % 10;
		val /= 10;
		buffer[--i] = d + '0';
	}

	if (i == 10)
		buffer[--i] = '0';
	if (sign)
		buffer[--i] = '-';

	return string(buffer + i, 10 - i);
}

string to_string(long val)
{
	char	buffer[12];

	bool sign = false;
	if (val < 0)
	{
		val = -val;
		sign = true;
	}

	char i = 12;

	while (val)
	{
		char d = val % 10;
		val /= 10;
		buffer[--i] = d + '0';
	}

	if (i == 12)
		buffer[--i] = '0';
	if (sign)
		buffer[--i] = '-';

	return string(buffer + i, 12 - i);
}

string to_string(unsigned int val)
{
	char	buffer[10];

	char i = 10;

	while (val)
	{
		char d = val % 10;
		val /= 10;
		buffer[--i] = d + '0';
	}

	if (i == 10)
		buffer[--i] = '0';

	return string(buffer + i, 10 - i);
}

string to_string(unsigned long val)
{
	char	buffer[12];

	char i = 12;

	while (val)
	{
		char d = val % 10;
		val /= 10;
		buffer[--i] = d + '0';
	}

	if (i == 12)
		buffer[--i] = '0';

	return string(buffer + i, 12 - i);
}

static float fround5[] = {
	0.5e-0, 0.5e-1, 0.5e-2, 0.5e-3, 0.5e-4, 0.5e-5, 0.5e-6
};

string to_string(float val)
{
	char	buffer[20];

	char	d = 0;

	float	f = val;

	if (f < 0.0)
	{
		f = -f;
		buffer[d++] = '-';
	}
		
	char prefix = d;

	if (isinf(f))
	{
		buffer[d++] = 'I';
		buffer[d++] = 'N';
		buffer[d++] = 'F';
	}
	else
	{
		int	exp = 0;

		char	fdigits = 6;

		if (f != 0.0)
		{
			while (f >= 1000.0)
			{
				f /= 1000;
				exp += 3;
			}

			while (f < 1.0)
			{
				f *= 1000;
				exp -= 3;
			}

			while (f >= 10.0)
			{
				f /= 10;
				exp ++;
			}
			
		}
		
		char digits = fdigits + 1;

		while (exp < 0)
		{
			f /= 10.0;
			exp++;
		}

		digits = fdigits + exp + 1;

		if (digits < 7)
			f += fround5[digits - 1];
		else
			f += fround5[6];

		if (f >= 10.0)
		{
			f /= 10.0;
			fdigits--;
		}			

		char	pdigits = digits - fdigits;

		if (digits > 20)
			digits = 20;

		if (pdigits == 0)
			buffer[d++] = '0';

		for(char i=0; i<digits; i++)
		{
			if (i == pdigits)
				buffer[d++] = '.';
			if (i > 6)
				buffer[d++] = '0';
			else
			{
				int c = (int)f;
				f -= (float)c;
				f *= 10.0;
				buffer[d++] = c + '0';
			}
		}
	}

	return string(buffer, d);
}


int string::to_int(char * idx, char base) const
{
	char i = 1;
	unsigned n = 0;
	bool sign = false;

	if (cstr)
	{
		const char * cp = cstr;

		char ch = cp[i];

		if (ch == '-')
		{
			sign = true;
			ch = cp[++i];
		}
		else if (ch == '+')
			ch = cp[++i];

		if (ch == '0' && base == 0)
		{
			ch = cp[++i];
			if (ch == 'x' || ch == 'X')	
			{
				base = 16;
				ch = cp[++i];
			}
			else if (ch == 'b' || ch == 'B')
			{
				base = 2;
				ch = cp[++i];			
			}
		}

		for(;;)
		{
			char d;
			if (ch >= '0' && ch <= '9')
				d = (ch - '0');
			else if (base > 10 && ch >= 'A' && ch <= 'F')
				d = (ch - 'A' + 10);
			else if (base > 10 && ch >= 'a' && ch <= 'f')
				d = (ch - 'a' + 10);
			else
				break;

			n = n * base + d;
			ch = cp[++i];
		}
	}

	if (idx)
		*idx = i - 1;

	if (sign)
		return -(int)n;
	else
		return n;
}

long string::to_long(char * idx, char base) const
{
	char i = 1;
	unsigned long n = 0;
	bool sign = false;

	if (cstr)
	{
		const char * cp = cstr;

		char ch = cp[i++];

		if (ch == '-')
		{
			sign = true;
			ch = cp[i++];
		}
		else if (ch == '+')
			ch = cp[i++];

		if (ch == '0' && base == 0)
		{
			ch = cp[i++];
			if (ch == 'x' || ch == 'X')	
			{
				base = 16;
				ch = cp[i++];
			}
			else if (ch == 'b' || ch == 'B')
			{
				base = 2;
				ch = cp[i++];			
			}
		}

		for(;;)
		{
			char d;
			if (ch >= '0' && ch <= '9')
				d = (ch - '0');
			else if (base > 10 && ch >= 'A' && ch <= 'F')
				d = (ch - 'A' + 10);
			else if (base > 10 && ch >= 'a' && ch <= 'f')
				d = (ch - 'a' + 10);
			else
				break;

			n = n * base + d;
			ch = cp[i++];
		}
		i--;
	}

	if (idx)
		*idx = i - 1;

	if (sign)
		return -(long)n;
	else
		return n;
}

unsigned string::to_uint(char * idx, char base) const
{
	char i = 1;
	unsigned n = 0;

	if (cstr)
	{
		const char * cp = cstr;

		char ch = cp[i];

		if (ch == '0' && base == 0)
		{
			ch = cp[++i];
			if (ch == 'x' || ch == 'X')	
			{
				base = 16;
				ch = cp[++i];
			}
			else if (ch == 'b' || ch == 'B')
			{
				base = 2;
				ch = cp[++i];			
			}
		}

		for(;;)
		{
			char d;
			if (ch >= '0' && ch <= '9')
				d = (ch - '0');
			else if (base > 10 && ch >= 'A' && ch <= 'F')
				d = (ch - 'A' + 10);
			else if (base > 10 && ch >= 'a' && ch <= 'f')
				d = (ch - 'a' + 10);
			else
				break;

			n = n * base + d;
			ch = cp[++i];
		}
	}

	if (idx)
		*idx = i - 1;

	return n;
}

unsigned long string::to_ulong(char * idx, char base) const
{
	char i = 1;
	unsigned long n = 0;

	if (cstr)
	{
		const char * cp = cstr;

		char ch = cp[i++];

		if (ch == '0' && base == 0)
		{
			ch = cp[i++];
			if (ch == 'x' || ch == 'X')	
			{
				base = 16;
				ch = cp[i++];
			}
			else if (ch == 'b' || ch == 'B')
			{
				base = 2;
				ch = cp[i++];			
			}
		}

		for(;;)
		{
			char d;
			if (ch >= '0' && ch <= '9')
				d = (ch - '0');
			else if (base > 10 && ch >= 'A' && ch <= 'F')
				d = (ch - 'A' + 10);
			else if (base > 10 && ch >= 'a' && ch <= 'f')
				d = (ch - 'a' + 10);
			else
				break;

			n = n * base + d;
			ch = cp[i++];
		}
		i--;
	}

	if (idx)
		*idx = i - 1;

	return n;
}

float string::to_float(char * idx) const
{
	char 	i = 1;
	float 	vf = 0;
	bool	sign = false;

	if (cstr)
	{
		const char * cp = cstr;

		char ch = cp[i++];
		if (ch == '-')
		{
			sign = true;
			ch = cp[i++];
		}
		else if (ch == '+')
			ch = cp[i++];
			
		if (ch >= '0' && ch <= '9' || ch == '.')
		{	
			while (ch >= '0' && ch <= '9')
			{
				vf = vf * 10 + (int)(ch - '0');
				ch = cp[i++];
			}

			if (ch == '.')
			{
				float	digits = 1.0;
				ch = cp[i++];
				while (ch >= '0' && ch <= '9')
				{
					vf = vf * 10 + (int)(ch - '0');
					digits *= 10;
					ch = cp[i++];
				}
				vf /= digits;
			}

			char	e = 0;
			bool	eneg = false;								
			
			if (ch == 'e' || ch == 'E')
			{
				ch = cp[i++];
				if (ch == '-')
				{
					eneg = true;
					ch = cp[i++];
				}
				else if (ch == '+')
				{
					ch = cp[i++];
				}
					
				while (ch >= '0' && ch <= '9')
				{
					e = e * 10 + ch - '0';
					ch = cp[i++];
				}
				
			}
			
			if (e)
			{
				if (eneg)
				{
					while (e > 6)
					{
						vf /= 1000000.0;
						e -= 6;
					}
					vf /= tpow10[e];
				}
				else
				{
					while (e > 6)
					{
						vf *= 1000000.0;
						e -= 6;
					}
					vf *= tpow10[e];
				}
			}
		}	
		i--;
	}

	if (idx)
		*idx = i - 1;

	if (sign)
		return -vf;
	else
		return vf;	
}

}
