#ifndef OPP_STRING_H
#define OPP_STRING_H

class string
{
private:
	char	*	cstr;

	friend void swap(string & u, string & v);

public:
	string(void);
	string(const string & s);
	string(string && s);
	string(const char * s);
	string(const char * s, char size);
	string(char c);
	~string(void);

	unsigned size(void) const;

	string & operator=(const string & s);
	string & operator=(string && s);
	string & operator=(const char * s);

	string & operator+=(const string & s);
	string & operator+=(const char * s);
	string & operator+=(char c);

	string operator+(const string & s) const;
	string operator+(const char * s) const;
	string operator+(char c) const;

	string & operator<<=(char n);
	string & operator>>=(char n);

	string operator<<(char n) const;
	string operator>>(char n) const;

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

	char & operator[](char t);
	char operator[](char t) const;

	const char * tocstr(void) const;

	string substr(char pos, char len) const;

	int find(const string & s) const;
	int find(const char * s) const;
	int find(char c) const;

	int find(const string & s, char pos) const;
	int find(const char * s, char pos) const;
	int find(char c, char pos) const;

	void copyseg(char * p, char at, char num) const;
protected:
	string(char l, char * b);
};

void swap(string & u, string & v);

#pragma compile("string.cpp")

#endif
