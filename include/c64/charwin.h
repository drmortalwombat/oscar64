#ifndef C64_CHARWIN_H
#define C64_CHARWIN_H

struct CharWin
{
	char		sx, sy, wx, wy;
	char		cx, cy;

	char	*	sp, * cp;
};

// Initialize the CharWin structure for the given screen and coordinates, does
// not clear the window
//
void cwin_init(CharWin * win, char * screen, char sx, char sy, char wx, char wy);


// Clear the window
//
void cwin_clear(CharWin * win);

// Fill the window with the given character and color
//
void cwin_fill(CharWin * win, char ch, char color);


// Show or hide the cursor by setting or clearing the MSB of the character code
//
void cwin_cursor_show(CharWin * win, bool show);

// Move the cursor to the given location
//
void cwin_cursor_move(CharWin * win, char cx, char cy);


// Move the cursor in the window, returns true if the cursor could be moved
//
bool cwin_cursor_left(CharWin * win);
bool cwin_cursor_right(CharWin * win);
bool cwin_cursor_up(CharWin * win);
bool cwin_cursor_down(CharWin * win);
bool cwin_cursor_forward(CharWin * win);
bool cwin_cursor_backward(CharWin * win);

// Read the full window into a string
//
void cwin_read_string(CharWin * win, char * buffer);

// Write the fill window with the given string
//
void cwin_write_string(CharWin * win, const char * buffer);

// Put a single char at the cursor location and advance the cursor
//
void cwin_put_char(CharWin * win, char ch, char color);

// Put an array of chars at the cursor location and advance the cursor
//
void cwin_put_chars(CharWin * win, const char * chars, char num, char color);

// Put a zero terminated string at the cursor location and advance the cursor
//
char cwin_put_string(CharWin * win, const char * str, char color);

// Put a single char at the given window location
//
void cwin_putat_char(CharWin * win, char x, char y, char ch, char color);

// Put an array of chars at the given window location
//
void cwin_putat_chars(CharWin * win, char x, char y, const char * chars, char num, char color);

// Put a zero terminated string at the given window location
//
char cwin_putat_string(CharWin * win, char x, char y, const char * str, char color);

// Insert one space character at the cursor position
//
void cwin_insert_char(CharWin * win);

// Delete the character at the cursor position
//
void cwin_delete_char(CharWin * win);

// Edit the window position using the char as the input
//
bool cwin_edit_char(CharWin * win, char ch);

// Edit the window using keyboard input, returns the key the exited
// the edit, either return or stop
//
char cwin_edit(CharWin * win);

// Scroll the window in the given direction, does not fill the new
// empty space
//
void cwin_scroll_left(CharWin * win, char by);
void cwin_scroll_right(CharWin * win, char by);
void cwin_scroll_up(CharWin * win, char by);
void cwin_scroll_down(CharWin * win, char by);

// Fill the given rectangle with the character and color
//
void cwin_fill_rect(CharWin * win, char x, char y, char w, char h, char ch, char color);

#pragma compile("charwin.c")

#endif
