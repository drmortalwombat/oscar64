#include "iostream.h"
#include <math.h>
#include <stdio.h>

namespace opp {

ios::ios(void)
	: mFlags(0), mState(0), mWidth(0), mPrecision(6), mFill(' ')
{}

inline ios::~ios(void)
{

}

char ios::fill(void) const
{
	return mFill;
}

char ios::fill(char fillch)
{
	char c = mFill; 
	mFill = fillch;
	return c;
}

char ios::width() const
{
	return mWidth;
}

char ios::width(char wide)
{
	char w = mWidth;
	mWidth = wide;
	return w;
}

char ios::precision() const
{
	return mPrecision;
}

char ios::precision(char wide)
{
	char w = mPrecision;
	mPrecision = wide;
	return w;
}

ios::fmtflags ios::flags(void) const
{
	return mFlags;
}

ios::fmtflags ios::flags(ios::fmtflags f)
{
	fmtflags pf = mFlags;
	mFlags = f;
	return pf;
}

ios::fmtflags ios::setf(ios::fmtflags f)
{
	fmtflags pf = mFlags;
	mFlags |= f;
	return pf;
}

ios::fmtflags ios::setf(ios::fmtflags f, ios::fmtflags m)
{
	fmtflags pf = mFlags;
	mFlags = (mFlags & ~m) | f;
	return pf;
}

ios::fmtflags ios::unsetf(ios::fmtflags f)
{
	fmtflags pf = mFlags;
	mFlags &= ~f;
	return pf;
}

bool ios::good(void) const
{
	return mState == goodbit;
}

bool ios::eof(void) const
{
	return (mState & eofbit) != 0;
}

bool ios::fail(void) const
{
	return (mState & (badbit | failbit)) != 0;
}

bool ios::bad(void) const
{
	return (mState & badbit) != 0;
}

bool ios::operator!(void) const
{
	return fail();
}

ios::operator bool(void)
{
	return !fail();
}


ios::statebits ios::rdstate(void) const
{
	return mState;
}

void ios::clear(void)
{
	mState = goodbit;
}




ostream & endl(ostream & os)
{
	os.put('\n');
	return os;
}


iosetf setf(ios::fmtflags flags)
{
	return iosetf(flags);
}

ostream & operator<<(ostream & os, const iosetf & s)
{
	os.setf(s.flags);
	return os;
}

iosetw setw(char width)
{
	return iosetw(width);
}

ostream & operator<<(ostream & os, const iosetw & s)
{
	os.width(s.width);
	return os;
}


iosetprecision setprecision(char precision)
{
	return iosetprecision(precision);
}

ostream & operator<<(ostream & os, const iosetprecision & s)
{
	os.precision(s.precision);
	return os;
}


iosetfill setfill(char fill)
{
	return iosetfill(fill);
}

ostream & operator<<(ostream & os, const iosetfill & s)
{
	os.fill(s.fill);
	return os;
}

inline ostream::ostream(void)
	{}


void ostream::bput(char ch)
{
}

ostream & ostream::put(char c)
{
	bput(c);
	return * this;	
}

ostream & ostream::write(const char * s, int n)
{
	for(int i=0; i<n; i++)
		bput(s[i]);

	return *this;
}

void ostream::putnum(const char * buffer, char prefix, char size)
{
	char r = 0;
	if (size < mWidth)
		r = mWidth - size;

	fmtflags	adj = mFlags & adjustfield;

	if (adj == internal)
	{
		while (size > prefix)
			bput(buffer[--size]);
	}

	if (adj != left)
	{
		while (r > 0)
		{
			bput(mFill);
			r--;
		}
	}

	while (size > 0)
		bput(buffer[--size]);

	while (r > 0)
	{
		bput(mFill);
		r--;
	}

	mFlags = 0;
	mWidth = 0;
	mFill = ' ';
}

void ostream::numput(unsigned n, char sign)
{
	char	buffer[10];

	char base = 10;
	if (mFlags & hex)
		base = 16;
	else if (mFlags & oct)
		base = 8;

	char i = 0;
	char o = 'a' - 10;
	if (mFlags & uppercase)
		o = 'A' - 10;

	unsigned	nt = n;
	while (nt)
	{
		char d = nt % base;
		nt /= base;

		if (d < 10)
			d += '0';
		else
			d += o;
		buffer[i++] = d;
	}

	if (!i)
		buffer[i++] = '0';
	char prefix = i;
	if (sign)
		buffer[i++] = sign;

	putnum(buffer, prefix, i);
}

void ostream::numput(unsigned long n, char sign)
{
	char	buffer[20];

	char base = 10;
	if (mFlags & hex)
		base = 16;
	else if (mFlags & oct)
		base = 8;

	char i = 0;
	char o = 'a' - 10;
	if (mFlags & uppercase)
		o = 'A' - 10;

	unsigned long nt = n;
	while (nt)
	{
		char d = nt % base;
		nt /= base;

		if (d < 10)
			d += '0';
		else
			d += o;
		buffer[i++] = d;
	}

	if (!i)
		buffer[i++] = '0';
	char prefix = i;
	if (sign)
		buffer[i++] = sign;

	putnum(buffer, prefix, i);
}


ostream & ostream::operator<<(bool val)
{
	if (val)
		bput('1');
	else
		bput('0');
	return *this;
}

ostream & ostream::operator<<(char val)
{
	bput(val);
	return *this;
}

ostream & ostream::operator<<(int val)
{
	if (val < 0)
		numput((unsigned)-val, '-');
	else if (mFlags & showpos)
		numput((unsigned)val, '+');
	else
		numput((unsigned)val, 0);
	return *this;
}

ostream & ostream::operator<<(unsigned val)
{
	numput(val, 0);
	return *this;
}

ostream & ostream::operator<<(long val)
{
	if (val < 0)
		numput(-val, '-');
	else if (mFlags & showpos)
		numput(val, '+');
	else
		numput(val, 0);
	return *this;
}

ostream & ostream::operator<<(unsigned long val)
{
	numput(val, 0);
	return *this;
}

static float fround5[] = {
	0.5e-0, 0.5e-1, 0.5e-2, 0.5e-3, 0.5e-4, 0.5e-5, 0.5e-6
};

ostream & ostream::operator<<(float val)
{
	char	buffer[20];

	char	d = 0;

	float	f = val;

	if (f < 0.0)
	{
		f = -f;
		buffer[d++] = '-';
	}
	else if (mFlags & showpos)
		buffer[d++] = '+';		
		
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

		char	fdigits = mPrecision;

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
		bool	fexp = mFlags & scientific;

		if (!(mFlags & fixed))
		{
			if (exp > 3 || exp < 0)
				fexp = true;
		}

		if (!fexp)
		{
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
		}
		else
		{
			if (digits < 7)
				f += fround5[digits - 1];
			else
				f += fround5[6];

			if (f >= 10.0)
			{
				f /= 10.0;
				exp ++;
			}
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

		if (fexp)
		{
			buffer[d++] = 'E';
			if (exp < 0)
			{
				buffer[d++] = '-';
				exp = -exp;
			}
			else
				buffer[d++] = '+';
			
			buffer[d++] = exp / 10 + '0'; 
			buffer[d++] = exp % 10 + '0';		
		}
	}

	char r = 0;
	if (d < mWidth)
		r = mWidth - d;

	fmtflags	adj = mFlags & adjustfield;

	char i = 0;
	if (adj == internal)
	{
		while (i < prefix)
			bput(buffer[i++]);
	}

	if (adj != left)
	{
		while (r > 0)
		{
			bput(mFill);
			r--;
		}
	}

	while (i < d)
		bput(buffer[i++]);

	while (r > 0)
	{
		bput(mFill);
		r--;
	}

	mFlags = 0;
	mWidth = 0;
	mFill = ' ';

	return *this;
}

ostream & ostream::operator<<(const char * p)
{
	char	i = 0;
	while (p[i])
		bput(p[i++]);
	return *this;
}

ostream & ostream::operator<<(const string & s)
{
	for(char i=0; i<s.size(); i++)
		bput(s[i]);
	return *this;
}

inline ostream & ostream::operator<<(manip m)
{
	return m(*this);
}


costream::costream(void)
{}

void costream::bput(char ch)
{
	putchar(ch);
}

istream::istream(void)
	: mBufferPos(0), mBufferFill(0)
{}

void istream::refill(void)
{

}

istream & istream::putback(char c)
{
	mBuffer[--mBufferPos] = c;
	return *this;
}

istream & istream::unget(void)
{
	if (!(mState & eofbit))
		mBufferPos--;
	return *this;
}

char istream::get(void)
{
	if (mState != goodbit)
	{
		mState |= failbit;
		return 0;
	}
	else if (mBufferPos == mBufferFill)
		refill();
	if (mBufferPos < mBufferFill)
		return mBuffer[mBufferPos++];
	else
	{
		mState |= eofbit;
		return 0;
	}
}


void istream::doskipws(void)
{
	while (char c = get())
	{
		if (c > ' ')
		{
			unget();
			return;
		}
	}
}



istream & istream::get(char & c)
{
	c = get();
	return *this;
}

istream & istream::get(char * s, char size)
{
	return get(s, size, '\n');
}

istream & istream::get(char * s, char size, char delim)
{
	char i = 0;
	while (i + 1 < size)
	{
		char c = get();
		if (c == delim)
		{
			unget();
			break;
		}
		s[i++] = c;
	}
	if (size)
		s[i++] = 0;
	return *this;
}

istream & istream::getline(char * s, char size)
{
	return getline(s, size, '\n');
}

istream & istream::getline(char * s, char size, char delim)
{
	char i = 0;
	while (i + 1 < size)
	{
		char c = get();
		if (c == delim)
			break;
		s[i++] = c;
	}
	if (size)
		s[i++] = 0;
	return *this;
}

istream & istream::ignore(char size)
{
	ignore(size, '\n');
	return *this;
}

istream & istream::ignore(char size, char delim)
{
	while (size)
	{
		char c = get();
		if (c == delim)
			break;
		size--;
	}

	return *this;
}

istream & istream::operator>>(bool & val)
{
	return *this;
}

unsigned istream::getnum(void)
{
	doskipws();

	fmtflags bflags = mFlags & basefield;

	char base = 10;
	if (bflags == hex)
		base = 16;
	else if (bflags == oct)
		base = 8;

	unsigned	n = 0;
	bool		sign = false;

	char ch = get();
	if (ch == '-')
	{
		sign = true;
		ch = get();
	}
	else if (ch == '+')
		ch = get();

	bool	digits = false;

	if (ch == '0')
	{
		if (bflags == 0)
		{
			base = 8;
			ch = get();
			if (ch == 'x' || ch == 'X')
			{
				base = 16;
				ch = get();
			}
			else
				digits = true;				
		}
		else
			digits = true;				
	}

	for(;;)
	{
		if (ch >= '0' && ch <= '9')
			n = n * base + (ch - '0');
		else if (base > 10 && ch >= 'A' && ch <= 'F')
			n = n * base + (ch - 'A' + 10);
		else if (base > 10 && ch >= 'a' && ch <= 'f')
			n = n * base + (ch - 'a' + 10);
		else
			break;
		ch = get();
		digits = true;
	}
	unget();

	if (!digits)
		mState |= failbit;

	if (sign)
		return -n;
	else
		return n;	
}

unsigned long istream::getnuml(void)
{
	doskipws();

	fmtflags bflags = mFlags & basefield;

	char base = 10;
	if (bflags == hex)
		base = 16;
	else if (bflags == oct)
		base = 8;

	unsigned	long n = 0;
	bool		sign = false;

	char ch = get();
	if (ch == '-')
	{
		sign = true;
		ch = get();
	}
	else if (ch == '+')
		ch = get();

	if (ch == '0')
	{
		if (bflags == 0)
		{
			base = 8;
			ch = get();
			if (ch == 'x' || ch == 'X')
			{
				base = 16;
				ch = get();
			}
		}
	}

	bool	digits = false;
	for(;;)
	{
		if (ch >= '0' && ch <= '9')
			n = n * base + (ch - '0');
		else if (base > 10 && ch >= 'A' && ch <= 'F')
			n = n * base + (ch - 'A' + 10);
		else if (base > 10 && ch >= 'a' && ch <= 'f')
			n = n * base + (ch - 'a' + 10);
		else
			break;
		ch = get();
		digits = true;
	}
	unget();

	if (!digits)
		mState |= failbit;

	if (sign)
		return -n;
	else
		return n;	
}

float istream::getnumf(void)
{
	doskipws();

	char cs = get();

	bool	sign = false;
	if (cs == '-')
	{
		sign = true;
		cs = get();
	}
	else if (cs == '+')
		cs = get();
		
	if (cs >= '0' && cs <= '9' || cs == '.')
	{	
		float	vf = 0;
		while (cs >= '0' && cs <= '9')
		{
			vf = vf * 10 + (int)(cs - '0');
			cs = get();
		}

		if (cs == '.')
		{
			float	digits = 1.0;
			cs = get();
			while (cs >= '0' && cs <= '9')
			{
				vf = vf * 10 + (int)(cs - '0');
				digits *= 10;
				cs = get();
			}
			vf /= digits;
		}

		char	e = 0;
		bool	eneg = false;								
		
		if (cs == 'e' || cs == 'E')
		{
			cs = get();
			if (cs == '-')
			{
				eneg = true;
				cs = get();
			}
			else if (cs == '+')
			{
				cs = get();
			}
				
			while (cs >= '0' && cs <= '9')
			{
				e = e * 10 + cs - '0';
				cs = get();
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

		if (sign)
			return -vf;
		else
			return vf;	
	}
	else
		mState |= failbit;

	return 0;
}

istream & istream::operator>>(int & val)
{
	val = getnum();
	return *this;
}

istream & istream::operator>>(unsigned & val)
{
	val = getnum();
	return *this;
}

istream & istream::operator>>(long & val)
{
	val = getnuml();
	return *this;
}

istream & istream::operator>>(unsigned long & val)
{
	val = getnuml();
	return *this;
}

istream & istream::operator>>(float & val)
{
	val = getnumf();
	return *this;
}

istream & istream::operator>>(char * p)
{
	doskipws();
	char i = 0;
	char c = get();
	while (c > ' ')
	{
		p[i++] = c;
		c = get();
	}
	return *this;
}

istream & istream::operator>>(string & s)
{
	doskipws();
	s.clear();
	char c = get();
	while (c > ' ')
	{
		s += c;
		c = get();
	}
	return *this;	
}

cistream::cistream(void)
{}

void cistream::refill(void)
{
	mBufferFill = 0;
	mBufferPos = 0;

	char ch;
	while (mBufferFill < 32)
	{
		char ch = getchar();
		mBuffer[mBufferFill++] = ch;
		if (ch == '\n')
			break;
	}
}

cistream	cin;
costream 	cout;

}