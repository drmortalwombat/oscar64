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


int kbhit(void);

int getche(void);

int getch(void);

// like getch but does not wait, returns zero if no
// key is pressed
int getchx(void);

void putch(int c);

void clrscr(void);

void gotoxy(int x, int y);

void textcolor(int c);

int wherex(void);

int wherey(void);

// show or hide the text cursor

void textcursor(bool show);

#pragma compile("conio.c")

#endif

