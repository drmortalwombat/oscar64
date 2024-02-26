#ifndef OPP_IOSTREAM_H
#define OPP_IOSTREAM_H

#include <opp/string.h>

namespace opp {

class ios
{
public:

	constexpr ios(void);
	virtual ~ios(void);

	char fill() const;
	char fill(char cillch);

	char width() const;
	char width(char wide);

	char precision() const;
	char precision(char prec);

	enum fmtflags
	{
		boolalpha	=	0x0001,
		dec			=	0x0002,
		fixed		=	0x0004,
		hex			=	0x0008,
		internal	=	0x0010,
		left		=	0x0020,
		oct			=	0x0040,
		right		=	0x0080,
		scientific	=	0x0100,
		showbase	=	0x0200,
		showpoint	=	0x0400,
		showpos		=	0x0800,
		skipws		=	0x1000,
		unitbuf		=	0x2000,
		uppercase	=	0x3000,

		adjustfield	=	0x00b0,
		basefield	=	0x004a,
		floatfield	=	0x0104
	};

	enum statebits
	{
		goodbit = 0,
		badbit = 1,
		eofbit = 2,
		failbit = 4,
	};

	fmtflags flags(void) const;
	fmtflags flags(fmtflags f);

	fmtflags setf(fmtflags f);
	fmtflags setf(fmtflags f, fmtflags m);
	fmtflags unsetf(fmtflags f);

	bool good(void) const;
	bool eof(void) const;
	bool fail(void) const;
	bool bad(void) const;

	bool operator!(void) const;
	operator bool(void);

	statebits rdstate(void) const;
	void clear(void);

protected:
	fmtflags 	mFlags;
	statebits	mState;
	char		mWidth;
	char		mPrecision;
	char		mFill;
};

class ostream;

typedef ostream & (* manip)(ostream &);

class ostream : public ios
{
public:
	constexpr ostream(void);

	ostream & put(char c);
	ostream & write(const char * s, int n);		

	ostream & operator<<(bool val);
	ostream & operator<<(char val);
	ostream & operator<<(int val);
	ostream & operator<<(unsigned val);
	ostream & operator<<(long val);
	ostream & operator<<(unsigned long val);
	ostream & operator<<(float val);

	ostream & operator<<(const char * p);
	ostream & operator<<(const string & s);

	ostream & operator<<(manip m);
protected:
	void putnum(const char * buffer, char prefix, char size);
	void numput(unsigned n, char sign);
	void numput(unsigned long n, char sign);

	virtual void bput(char ch);
};

class istream : public ios
{
public:
	char get(void);
	istream & get(char & c);
	istream & get(char * s, char size);
	istream & get(char * s, char size, char delim);
	istream & getline(char * s, char size);
	istream & getline(char * s, char size, char delim);
	istream & ignore(char size);
	istream & ignore(char size, char delim);
	istream & putback(char c);
	istream & unget(void);

	istream & operator>>(char & val);
	istream & operator>>(bool & val);
	istream & operator>>(int & val);
	istream & operator>>(unsigned & val);
	istream & operator>>(long & val);
	istream & operator>>(unsigned long & val);
	istream & operator>>(float & val);

	istream & operator>>(char * p);
	istream & operator>>(string & s);

	istream(void);
protected:
	char	mBuffer[32];
	char	mBufferPos, mBufferFill;

	virtual void refill(void);

	unsigned getnum(void);
	unsigned long getnuml(void);
	float getnumf(void);

	void doskipws(void);
};

class costream : public ostream
{
public:
	constexpr costream(void);

protected:
	void bput(char ch);
};

class cistream : public istream
{
public:
	cistream(void);

protected:
	void refill(void);
};

ostream & endl(ostream & os);

struct iosetf {
	ios::fmtflags flags;
	iosetf(ios::fmtflags flags_) : flags(flags_) {}
};

ostream & operator<<(ostream & os, const iosetf & s);

iosetf setf(ios::fmtflags flags);

struct iosetw {
	char width;
	iosetw(char width_) : width(width_) {}
};

iosetw setw(char width);

ostream & operator<<(ostream & os, const iosetw & s);

struct iosetprecision {
	char precision;
	iosetprecision(char precision_) : precision(precision_) {}
};

iosetprecision setprecision(char precision);

ostream & operator<<(ostream & os, const iosetprecision & s);


struct iosetfill {
	char fill;
	iosetfill(char fill_) : fill(fill_) {}
};

iosetfill setfill(char fill);

ostream & operator<<(ostream & os, const iosetfill & s);


extern cistream	cin;
extern costream cout;

}

#pragma compile("iostream.cpp");

#endif
