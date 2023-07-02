#ifndef OPP_STRING_H
#define OPP_STRING_H

class string
{
private:
	char	*	cstr;

public:
	string(void);
	string(const string & s);
	string(const char * s);
	string(const char * s1, const char * s2);
	~string(void);

	unsigned size(void) const;
	
	string & operator=(const string & s);
	string & operator=(const char * s);

	string & operator+=(const string & s);
	string & operator+=(const char * s);

	string operator+(const string & s);
	string operator+(const char * s);

	bool operator==(const string & s) const;
	bool operator==(const char * s) const;
	bool operator!=(const string & s) const;
	bool operator!=(const char * s) const;

	bool operator<(const string & s) const;
	bool operator<(const char * s) const;
	bool operator<=(const string & s) const;
	bool operator<=(const char * s) const;

	bool operator>(const string & s) const;
	bool operator>(const char * s) const;
	bool operator>=(const string & s) const;
	bool operator>=(const char * s) const;

	char & operator[](unsigned t);
	char operator[](unsigned t) const;

	const char * tocstr(void) const;
};

#pragma compile("string.cpp")

#endif
