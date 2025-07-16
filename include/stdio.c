#include "stdio.h"
#include "conio.h"
#include "stdlib.h"


void putchar(char c)
{
	putpch(c);
}

char getchar(void)
{
	return getpch();
}

void puts(const char * str)
{
	while (char ch = *str++)
		putpch(ch);
}

char * gets(char * str)
{
	char i = 0;
	while ((char ch = getpch()) != '\n')
		str[i++] = ch;
	str[i] = 0;
	return str;
}

char * gets_s(char * str, size_t n)
{
	if (str == NULL)
		return NULL;

	if (n < 2)
		return NULL;
	char i = 0, t = n - 1;

	while ((char ch = getpch()) != '\n')
	{
		if (i < t)
			str[i] = ch;
		++i;
	}
	str[(i < t) ? i : t] = '\0';

	if (i > t)
		return NULL;
	return str;
}

typedef void * (* putstrfn)(void * handle, const char * str);

void * putstrio(void * handle, const char * str)
{
	puts(str);
	return handle;
}

void * putstrstr(void * handle, const char * str)
{
	char * d = (char *)handle;
	const char * s = str;

	do {} while (*d++ = *s++);

	return d - 1;
}

struct sinfo
{
	char		fill;
	char		width, precision;
	unsigned	base;
	bool		sign, left, prefix;
};

int nformi(const sinfo * si, char * str, int v, bool s)
{
	char	buffer[16];

	unsigned int u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	char	i = 16;
	while (u > 0)
	{
		int	c = u % si->base;
		c += c >= 10 ? 'A' - 10 : '0';
		buffer[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 16 - si->precision : 15;

	while (i > digits)
		buffer[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		buffer[--i] = 'X';
		buffer[--i] = '0';
	}

	if (neg)
		buffer[--i] = '-';
	else if (si->sign)
		buffer[--i] = '+';

	char j = 0;
	if (si->left)
	{
		while (i < 16)
			str[j++] = buffer[i++];
		while (j < si->width)
			str[j++] = si->fill;
	}
	else
	{
		while (i + si->width > 16)
			buffer[--i] = si->fill;
		while (i < 16)
			str[j++] = buffer[i++];
	}

	return j;
}

int nforml(const sinfo * si, char * str, long v, bool s)
{
	char	buffer[16];

	unsigned long u = v;
	bool	neg = false;

	if (s && v < 0)
	{
		neg = true;
		u = -v;
	}

	char	i = 16;

	while (u > 0)
	{
		int	c = u % si->base;
		c += c >= 10 ? 'A' - 10 : '0';
		buffer[--i] = c;
		u /= si->base;
	}

	char	digits = si->precision != 255 ? 16 - si->precision : 15;

	while (i > digits)
		buffer[--i] = '0';

	if (si->prefix && si->base == 16)
	{
		buffer[--i] = 'X';
		buffer[--i] = '0';
	}

	if (neg)
		buffer[--i] = '-';
	else if (si->sign)
		buffer[--i] = '+';

	char j = 0;
	if (si->left)
	{
		while (i < 16)
			str[j++] = buffer[i++];
		while (j < si->width)
			str[j++] = si->fill;
	}
	else
	{
		while (i + si->width > 16)
			buffer[--i] = si->fill;
		while (i < 16)
			str[j++] = buffer[i++];
	}

	return j;
}

static float fround5[] = {
	0.5e-0, 0.5e-1, 0.5e-2, 0.5e-3, 0.5e-4, 0.5e-5, 0.5e-6
};

int nformf(const sinfo * si, char * str, float f, char type)
{
	char 	* 	sp = str;

	char	d = 0;

	if (f < 0.0)
	{
		f = -f;
		sp[d++] = '-';
	}
	else if (si->sign)
		sp[d++] = '+';		
		
	if (isinf(f))
	{
		sp[d++] = 'I';
		sp[d++] = 'N';
		sp[d++] = 'F';
	}
	else
	{
		int	exp = 0;

		char	fdigits = si->precision != 255 ? si->precision : 6;

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
		bool	fexp = type == 'e';

		if (type == 'g')
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
			sp[d++] = '0';

		for(char i=0; i<digits; i++)
		{
			if (i == pdigits)
				sp[d++] = '.';
			if (i > 6)
				sp[d++] = '0';
			else
			{
				int c = (int)f;
				f -= (float)c;
				f *= 10.0;
				sp[d++] = c + '0';
			}
		}

		if (fexp)
		{
			sp[d++] = 'E';
			if (exp < 0)
			{
				sp[d++] = '-';
				exp = -exp;
			}
			else
				sp[d++] = '+';
			
			sp[d++] = exp / 10 + '0'; 
			sp[d++] = exp % 10 + '0';		
		}
	}

	if (d < si->width)
	{
		if (si->left)
		{
			for(char i=d; i<si->width; i++)
				sp[i] = ' ';
		}
		else
		{
			for(char i=1; i<=d; i++)
				sp[si->width - i] = sp[d - i];
			for(char i=0; i<si->width-d; i++)
				sp[i] = ' ';
		}
		d = si->width;
	}

	return d;
}

char * sformat(char * buff, const char * fmt, int * fps, bool print)
{
	const char	*	p = fmt;
	char 	* 	bp = buff;
	char		c;
	char		bi = 0;
	sinfo		si;
	
	while (c = *p++)
	{
		if (c == '%')
		{			
			if (bi)
			{
				if (print)
				{
					bp[bi] = 0;
					puts(bp);
				}
				else
					bp += bi;
				bi = 0;
			}
			c = *p++;

			si.base = 10;
			si.width = 0;
			si.precision = 255;
			si.fill = ' ';
			si.sign = false;
			si.left = false;
			si.prefix = false;

			while(true)
			{
				if (c == '+')
					si.sign = true;
				else if (c == '0')
					si.fill = '0';
				else if (c == '#')
					si.prefix = true;
				else if (c == '-')
					si.left = true;
				else					
					break;
				c = *p++;
			}

			if (c >= '0' && c <='9')
			{
				char i = 0;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}
				si.width = i;
			}

			if (c == '.')
			{
				char	i = 0;
				c = *p++;
				while (c >= '0' && c <='9')
				{
					i = i * 10 + c - '0';
					c = *p++;
				}		
				si.precision = i;		
			}

			if (c == 'd' || c == p'd' || c == 'i' || c == p'i')
			{
				bi = nformi(&si, bp, *fps++, true);
			}
			else if (c == 'u' || c == p'u')
			{
				bi = nformi(&si, bp, *fps++, false);
			}
			else if (c == 'x' || c == p'x')
			{
				si.base = 16;
				bi = nformi(&si, bp, *fps++, false);
			}
#ifndef NOLONG
			else if (c == 'l' || c == p'l')
			{
				long l = *(long *)fps;
				fps ++;
				fps ++;

				c = *p++;
				if (c == 'd' || c == p'd')
				{
					bi = nforml(&si, bp, l, true);
				}
				else if (c == 'u' || c == p'u')
				{
					bi = nforml(&si, bp, l, false);
				}
				else if (c == 'x' || c == p'x')
				{
					si.base = 16;
					bi = nforml(&si, bp, l, false);
				}
			}
#endif
#ifndef NOFLOAT
			else if (c == 'f' || c == 'g' || c == 'e' || c == p'f' || c == p'g' || c == p'e')
			{
				bi = nformf(&si, bp, *(float *)fps, c);
				fps ++;
				fps ++;
			}
#endif			
			else if (c == 's' || c == p's')
			{
				char * sp = (char *)*fps++;

				char n = 0;
				if (si.width)
				{
					while (sp[n])
						n++;
				}

				if (!si.left)
				{
					while (n < si.width)
					{
						bp[bi++]  = si.fill;
						n++;
					}
				}

				if (print)
				{
					if (bi)
					{
						bp[bi] = 0;
						puts(bp);
						bi = 0;
					}
					puts(sp);
				}
				else
				{
					while (char c = *sp++)
						bp[bi++] = c;

				}

				if (si.left)
				{
					while (n < si.width)
					{
						bp[bi++] = si.fill;
						n++;
					}						
				}
			}
			else if (c == 'c' || c == p'c')
			{
				bp[bi++] = *fps++;
			}
			else if (c)
			{
				bp[bi++] = c;
			}
		}
		else
		{
			bp[bi++] = c;
			if (bi >= 40)
			{
				if (print)
				{
					bp[bi] = 0;
					puts(bp);
				}
				else
					bp += bi;
				bi = 0;
			}
		}
	}
	bp[bi] = 0;
	if (bi)
	{
		if (print)
			puts(bp);
		else
			bp += bi;
	}

	return bp;
}

void printf(const char * fmt, ...)
{
	char	buff[50];
	sformat(buff, fmt, (int *)&fmt + 1, true);
}

int sprintf(char * str, const char * fmt, ...)
{
	char * d = sformat(str, fmt, (int *)&fmt + 1, false);
	return d - str;
}

void vprintf(const char * fmt, va_list vlist)
{
	char	buff[50];
	sformat(buff, fmt, (int *)vlist, true);
}

int vsprintf(char * str, const char * fmt, va_list vlist)
{
	char * d = sformat(str, fmt, (int *)vlist, false);
	return d - str;
}

static inline bool isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int hexch(char cs)
{
	if (cs >= '0' && cs <= '9')
		return cs - '0';
	else if (cs >= 'a' && cs <= 'f')
		return cs - 'a' + 10;
	else if (cs >= 'A' && cs <= 'F')
		return cs - 'A' + 10;
	else
		return -1;
}

static const char * scanpat(const char * fmt, char * mask)
{
	for(char i=0; i<32; i++)
		mask[i] = 0;
	
	bool	negated = false;
	char fc = *fmt++;
	
	if (fc == '^')
	{
		negated = true;
		fc = *fmt++;
	}
	
	do
	{
		char fd = fc;		
		char nc = *fmt++;
		
		if (nc == '-')
		{
			nc = *fmt;
			if (nc != ']')
			{	
				fmt++;
				fd = nc;
				nc = *fmt++;
			}
		}
		
		while (fc <= fd)
		{
			mask[fc >> 3] |= 0x01 << (fc & 7);
			fc++;
		}
		
		fc = nc;
				
	} while (fc && fc != ']');

	if (negated)
	{
		for(char i=4; i<32; i++)
			mask[i] = ~mask[i];
	}
	
	return fmt;
}

int fpscanf(const char * fmt, int (* ffunc)(void * p), void * fparam, void ** params)
{
	char		fc, cs;
	int			nv = 0;
	unsigned	nch = 0;
	
	cs = ffunc(fparam);
	nch++;

	while (cs > 0 && (fc = *fmt++))
	{
		switch (fc)
		{
			case ' ':
				while (cs > 0 && isspace(cs))
				{
					cs = ffunc(fparam);
					nch++;
				}
				break;
			case '%':		
			{
				int			width = 0x7fff;
				bool		issigned = true;
				bool		ignore = false;
				bool		islong = false;
				unsigned 	base = 10;
				
				fc = *fmt++;
				if (fc == '*')
				{
					ignore = true;
					fc = *fmt++;
				}				
				
				if (fc >= '0' && fc <= '9')
				{
					width = (int)(fc - '0');
					fc = *fmt++;
					while (fc >= '0' && fc <= '9')
					{
						width = width * 10 + (int)(fc - '0');
						fc = *fmt++;
					}
				}
				
				if (fc == 'l' || fc == p'l')
				{
					islong = true;
					fc = *fmt++;
				}				
				
				switch (fc)
				{
					case 'n':
						*(unsigned *)*params = nch;
						params++;					
						nv++;
						break;

					case 'x':
					case p'x':
						base = 16;
					case 'u':
					case p'u':
						issigned = false;
					case 'i':
					case 'd':
					case p'i':
					case p'd':
					{
						bool	sign = false;
						if (cs == '-')
						{
							sign = true;
							cs = ffunc(fparam);
							nch++;
						}
						else if (cs == '+')
						{
							cs = ffunc(fparam);
							nch++;
						}
						
						int cv;
						if ((cv = hexch(cs)) >= 0)
						{	
							cs = ffunc(fparam);
							nch++;

							if (cv == 0 && cs == 'x')
							{
								base = 16;
								cs = ffunc(fparam);
								nch++;
							}
#ifndef NOLONG
							if (islong)
							{
								unsigned long vi = (unsigned long)cv;
								while ((cv = hexch(cs)) >= 0)
								{
									vi = vi * base + (unsigned long)cv;
									cs = ffunc(fparam);
									nch++;
								}
							
								if (!ignore)
								{
									if (sign && issigned)
										*(long *)*params = -(long)vi;
									else
										*(unsigned long *)*params = vi;
										
									params++;					
									nv++;
								}
							}
							else
#endif							
							{
								unsigned	vi = (unsigned)cv;
								while ((cv = hexch(cs)) >= 0)
								{
									vi = vi * base + (unsigned)cv;
									cs = ffunc(fparam);
									nch++;
								}
							
								if (!ignore)
								{
									if (sign && issigned)
										*(int *)*params = -(int)vi;
									else
										*(unsigned *)*params = vi;
										
									params++;					
									nv++;
								}
							}
						}
						else
							return nv;
						
					} break;
#ifndef NOFLOAT
					
					case 'f':
					case 'e':
					case 'g':
					case p'f':
					case p'e':
					case p'g':
					{
						bool	sign = false;
						if (cs == '-')
						{
							sign = true;
							cs = ffunc(fparam);
							nch++;
						}
						else if (cs == '+')
						{
							cs = ffunc(fparam);
							nch++;
						}
							
						if (cs >= '0' && cs <= '9' || cs == '.')
						{	
							float	vf = 0;
							while (cs >= '0' && cs <= '9')
							{
								vf = vf * 10 + (int)(cs - '0');
								cs = ffunc(fparam);
								nch++;
							}

							if (cs == '.')
							{
								float	digits = 1.0;
								cs = ffunc(fparam);
								while (cs >= '0' && cs <= '9')
								{
									vf = vf * 10 + (int)(cs - '0');
									digits *= 10;
									cs = ffunc(fparam);
									nch++;
								}
								vf /= digits;
							}

							char	e = 0;
							bool	eneg = false;								
							
							if (cs == 'e' || cs == 'E')
							{
								cs = ffunc(fparam);
								nch++;
								if (cs == '-')
								{
									eneg = true;
									cs = ffunc(fparam);
									nch++;									
								}
								else if (cs == '+')
								{
									cs = ffunc(fparam);
									nch++;
								}
									
								while (cs >= '0' && cs <= '9')
								{
									e = e * 10 + cs - '0';
									cs = ffunc(fparam);
									nch++;
								}
								
							}
							
							if (!ignore)
							{
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
									*(float *)*params = -vf;
								else
									*(float *)*params = vf;
									
								params++;					
								nv++;
							}
							
						}
						else
							return nv;
						
					} break;
#endif
					case 's':
					case p's':
					{
						char	*	pch = (char *)*params;
						while (width > 0 && cs > 0 && !isspace(cs))
						{
							if (!ignore)
								*pch++ = cs;
							cs = ffunc(fparam);
							nch++;
							width--;
						}
						if (!ignore)
						{
							*pch = 0;
							params++;					
							nv++;
						}
					} break;

					case '[':
					{
						char	pat[32];
						fmt = scanpat(fmt, pat);
						
						char	*	pch = (char *)*params;
						while (width > 0 && (pat[cs >> 3] & (1 << (cs & 7))))
						{
							if (!ignore)
								*pch++ = cs;
							cs = ffunc(fparam);
							nch++;
							width--;
						}
						if (!ignore)
						{
							*pch = 0;
							params++;					
							nv++;
						}
						
					}	break;

					case 'c':
					case p'c':
					{
						char	*	pch = (char *)*params;
						if (!ignore)
						{
							*pch = cs;
							params++;					
							nv++;
						}
						cs = ffunc(fparam);
						nch++;
					} break;
				}
				
			} break;
			
				
			default:
				if (cs == fc)
				{
					cs = ffunc(fparam);
					nch++;
				}
				else
					return nv;					
				break;
		}		
	}
	
	return nv;
}


int sscanf_func(void * fparam)
{
	char ** cp = (char **)fparam;
	char c = **cp;
	if (c)
		(*cp)++;
	return c;
}

int scanf_func(void * fparam)
{
	return getchar();
}

int sscanf(const char * str, const char * fmt, ...)
{
	return fpscanf(fmt, sscanf_func, &str, (void **)((&fmt) + 1));
}

int scanf(const char * fmt, ...)
{
	return fpscanf(fmt, scanf_func, nullptr, (void **)((&fmt) + 1));
}

#if defined(__CBM__)
#include <c64/kernalio.h>


#define FNUM_BASE	2

struct FILE
{
	signed char	fnum;
};

FILE	files[FOPEN_MAX];
FILE 	stdio_file = {-1};

FILE * stdin = &stdio_file;
FILE * stdout = &stdio_file;

FILE * fopen(const char * fname, const char * mode)
{
	char i = 0;
	while (i < FOPEN_MAX && files[i].fnum)
		i++;

	if (i < FOPEN_MAX)
	{
		char cbmname[32];
		char j = 0, rj = 0;
		char pdrive = 0, pdevice = 8, t = 0;

		while (fname[j] >= '0' && fname[j] <= '9')
			t = t * 10 + (fname[j++] - '0');
		if (fname[j] == ':')
		{
			j++;
			pdrive = t;

			rj = j;
			t = 0;
			while (fname[j] >= '0' && fname[j] <= '9')
				t = t * 10 + (fname[j++] - '0');
			if (fname[j] == ':')
			{
				j++;
				pdevice = pdrive;
				pdrive = t;
			}
			else
				j = rj;
		}
		else
			j = rj;

		char k = 1, s = 1;
		if (pdrive >= 10)
		{
			cbmname[k++] = pdrive / 10 + '0';
			pdrive %= 10;
		}
		cbmname[k++] = pdrive + '0';
		cbmname[k++] = ':';
		while (fname[j])
			cbmname[k++] = fname[j++];
		cbmname[k++] = ',';
		cbmname[k++] = p's';
		cbmname[k++] = ',';
		if (mode[0] == 'w' || mode[0] == 'W')
		{
			cbmname[k++] = p'w';
			cbmname[--s] = p'@';
		}
		else if (mode[0] == 'r' || mode[0] == 'R')
			cbmname[k++] = p'r';
		else if (mode[0] == 'a' || mode[0] == 'A')
			cbmname[k++] = p'a';
		cbmname[k++] = 0;

		krnio_setnam(cbmname + s);
		if (krnio_open(i + FNUM_BASE, pdevice, i + FNUM_BASE))
		{
			files[i].fnum = i + FNUM_BASE;
			return files + i;
		}
	}

	return nullptr;
}

int fclose(FILE * fp)
{
	krnio_close(fp->fnum);
	fp->fnum = 0;
	return 0;
}

int fgetc(FILE* stream)
{
	if (stream->fnum >= 0)
		return krnio_getch(stream->fnum);
	else
		return getpch();
}

char* fgets(char* s, int n, FILE* stream)
{
	if (krnio_gets(stream->fnum, s, n) >= 0)
		return s;
	return nullptr;
}

int fputc(int c, FILE* stream)
{
	if (stream->fnum >= 0)
		return krnio_putch(stream->fnum);
	else
	{
		putpch(c);
		return 0;
	}

}

int fputs(const char* s, FILE* stream)
{
	if (stream->fnum >= 0)
		return krnio_puts(stream->fnum, s);	
	else
	{
		puts(s);
		return 0;
	}
}

int feof(FILE * stream)
{
	return stream->fnum >= 0 && krnio_pstatus[stream->fnum] == KRNIO_EOF;
}

size_t fread( void * buffer, size_t size, size_t count, FILE * stream )
{
	return krnio_read(stream->fnum, (char *)buffer, size * count) / size;
}

size_t fwrite( const void* buffer, size_t size, size_t count, FILE* stream )
{
	return krnio_write(stream->fnum, (const char *)buffer, size * count) / size;	
}

int fprintf( FILE * stream, const char* format, ... )
{
	char	buff[50];
	if (stream->fnum < 0 || krnio_chkout(stream->fnum))
	{
		sformat(buff, format, (int *)&format + 1, true);
		krnio_clrchn();
		return 0;
	}
	else
		return -1;
}

int fscanf_func(void * fparam)
{
	char ch = krnio_chrin();
	return ch;
}

int fscanf( FILE *stream, const char *format, ... )
{
	if (stream->fnum < 0 || krnio_chkin	(stream->fnum))
	{
		int res = fpscanf(format, fscanf_func, nullptr, (void **)((&format) + 1));
		krnioerr err = krnio_status();
		krnio_pstatus[stream->fnum] = err;
		krnio_clrchn();
		return res;
	}
	else
		return -1;
}

#endif

