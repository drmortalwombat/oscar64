#ifndef CONIO_H
#define CONIO_H

int kbhit(void);

int getche(void);

int getch(void);

void putch(int c);

void clrscr(void);

void gotoxy(int x, int y);

void textcolor(int c);

int wherex(void);

int wherey(void);

#pragma compile("conio.c")

#endif

