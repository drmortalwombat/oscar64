#include "string.h"
#include <string.h>
#include <stdlib.h>

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
		cstr = c;
	}
	else
	{
		cstr = malloc(3);
		cstr[0] = 1;
		cstr[1] = c;
		cstr[2] = 0;
	}	
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

string string::substr(char pos, char len) const
{
	if (!cstr || len == 0 || pos >= cstr[0])
		return string;
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
