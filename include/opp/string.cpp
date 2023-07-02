#include "string.h"
#include <string.h>
#include <stdlib.h>

string::string(void) : cstr(nullptr)
{}

string::string(const string & s)
{
	cstr = malloc(strlen(s.cstr) + 1);
	strcpy(cstr, s.cstr);
}

string::string(const char * s)
{
	cstr = malloc(strlen(s) + 1);
	strcpy(cstr, s);
}

string::string(const char * s1, const char * s2)
{
	cstr = malloc(strlen(s1) + strlen(s2) + 1);
	strcpy(cstr, s1);
	strcat(cstr, s2);
}

string::~string(void)
{
	free(cstr);
}
	
string & string::operator=(const string & s)
{
	if (cstr != s.cstr)	
	{
		free(cstr);
		cstr = malloc(strlen(s.cstr) + 1);
		strcpy(cstr, s.cstr);
	}

	return *this;
}

string & string::operator=(const char * s)
{
	free(cstr);
	cstr = malloc(strlen(s) + 1);
	strcpy(cstr, s);

	return *this;	
}

string & string::operator+=(const string & s)
{
	char * nstr = malloc(strlen(cstr) + strlen(s.cstr) + 1);
	strcpy(nstr, cstr);
	strcat(nstr, s.cstr);
	free(cstr);
	cstr = nstr;
	return *this;
}

string & string::operator+=(const char * s)
{
	char * nstr = malloc(strlen(cstr) + strlen(s) + 1);
	strcpy(nstr, cstr);
	strcat(nstr, s);
	free(cstr);
	cstr = nstr;
	return *this;
}

inline const char * string::tocstr(void) const
{
	return cstr;
}

inline unsigned string::size(void) const
{
	return strlen(cstr);
}

string string::operator+(const string & s)
{
	return string(cstr, s.cstr);
}

string string::operator+(const char * s)
{
	return string(cstr, s);
}


inline bool string::operator==(const string & s) const
{
	return strcmp(cstr, s.cstr) == 0;
}

inline bool string::operator==(const char * s) const
{
	return strcmp(cstr, s) == 0;
}

inline bool string::operator!=(const string & s) const
{
	return strcmp(cstr, s.cstr) != 0;
}

inline bool string::operator!=(const char * s) const
{
	return strcmp(cstr, s) == 0;
}

inline bool string::operator<(const string & s) const
{
	return strcmp(cstr, s.cstr) < 0;
}

inline bool string::operator<(const char * s) const
{
	return strcmp(cstr, s) < 0;
}

inline bool string::operator<=(const string & s) const
{
	return strcmp(cstr, s.cstr) <= 0;
}

inline bool string::operator<=(const char * s) const
{
	return strcmp(cstr, s) <= 0;
}

inline bool string::operator>(const string & s) const
{
	return strcmp(cstr, s.cstr) > 0;
}

inline bool string::operator>(const char * s) const
{
	return strcmp(cstr, s) > 0;
}

inline bool string::operator>=(const string & s) const
{
	return strcmp(cstr, s.cstr) >= 0;
}

inline bool string::operator>=(const char * s) const
{
	return strcmp(cstr, s) >= 0;
}

inline char & string::operator[](unsigned t)
{
	return cstr[t];
}

inline char string::operator[](unsigned t) const
{
	return cstr[t];
}
