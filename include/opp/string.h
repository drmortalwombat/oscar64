#ifndef OPP_STRING_H
#define OPP_STRING_H

namespace opp {

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

	void clear(void);

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

	char * begin(void);
	const char * begin(void) const;
	const char * cbegin(void) const;

	char * end(void);
	const char * end(void) const;
	const char * cend(void) const;

	const char * c_str(void) const;
	const char * tocstr(void) const;

	string substr(char pos, char len) const;

	int find(const string & s) const;
	int find(const char * s) const;
	int find(char c) const;

	int find(const string & s, char pos) const;
	int find(const char * s, char pos) const;
	int find(char c, char pos) const;

	void copyseg(char * p, char at, char num) const;

	int to_int(char * idx = nullptr, char base = 10) const;
	long to_long(char * idx = nullptr, char base = 10) const;
	unsigned to_uint(char * idx = nullptr, char base = 10) const;
	unsigned long to_ulong(char * idx = nullptr, char base = 10) const;
	float to_float(char * idx = nullptr) const;
protected:
	string(char l, char * b);
};

void swap(string & u, string & v);

string to_string(int val);

string to_string(long val);

string to_string(unsigned int val);

string to_string(unsigned long val);

string to_string(float val);

}

#pragma compile("string.cpp")

#endif
