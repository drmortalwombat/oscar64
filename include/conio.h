#ifndef CONIO_H
#define CONIO_H

enum IOCharMap
{
	IOCHM_TRANSPARENT,
	IOCHM_ASCII,
	IOCHM_PETSCII_1,
	IOCHM_PETSCII_2
};

extern IOCharMap	giocharmap;

// Switch character map to transparent bypass, petscii font 1 or
// petscii font 2.  Translation is performed for all reading and
// writing operations.  The ascii mode will only translate the
// line end CR into an LF

void iocharmap(IOCharMap chmap);

#if defined(__C128__) || defined(__C128B__) || defined(__C128E__)
void dispmode40col(void);
void dispmode80col(void);
#endif

#define PETSCII_CURSOR_LEFT		0x9d
#define PETSCII_CURSOR_RIGHT	0x1d
#define PETSCII_CURSOR_UP		0x91
#define PETSCII_CURSOR_DOWN		0x11
#define PETSCII_HOME			0x13
#define PETSCII_CLEAR			0x94
#define PETSCII_DEL				0x14
#define PETSCII_INSERT			0x94
#define PETSCII_STOP			0x0c
#define PETSCII_RETURN			0x0d

#define PETSCII_F1				0x85
#define PETSCII_F2				0x89
#define PETSCII_F3				0x86
#define PETSCII_F4				0x8a
#define PETSCII_F5				0x87
#define PETSCII_F6				0x8b
#define PETSCII_F7				0x88
#define PETSCII_F8				0x8c

char kbhit(void);

char getche(void);

char getch(void);

// like getch but does not wait, returns zero if no
// key is pressed
char getchx(void);

void putch(char c);

void clrscr(void);

void gotoxy(char x, char y);

void textcolor(char c);

char wherex(void);

char wherey(void);

// show or hide the text cursor

void textcursor(bool show);

#pragma compile("conio.c")

#endif

