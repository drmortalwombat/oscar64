#include "charwin.h"
#include <stdio.h>

static const unsigned mul40[25] = {
	  0,  40,  80, 120, 160,
	200, 240, 280, 320, 360,
	400, 440, 480, 520, 560,
	600, 640, 680, 720, 760,
	800, 840, 880, 920, 960
};

static __native inline void copy_fwd(char * sdp, const char * ssp, char * cdp, const char * csp, char n)
{
	for(char i=0; i<n; i++)
	{
		sdp[i] = ssp[i];
		cdp[i] = csp[i];
	}
}

static __native inline void fill_fwd(char * sdp, char * cdp, char ch, char color, char n)
{
	for(char i=0; i<n; i++)
	{
		sdp[i] = ch;
		cdp[i] = color;
	}
}

static __native inline void copy_bwd(char * sdp, const char * ssp, char * cdp, const char * csp, char n)
{
	while (n)
	{
		n--;
		sdp[n] = ssp[n];
		cdp[n] = csp[n];
	}
}


void cwin_init(CharWin * win, char * screen, char sx, char sy, char wx, char wy)
{
	win->sx = sx;
	win->sy = sy;
	win->wx = wx;
	win->wy = wy;
	win->cx = 0;
	win->cy = 0;
	win->sp = screen + mul40[sy] + sx;
	win->cp = (char *)0xd800 + mul40[sy] + sx;
}


void cwin_clear(CharWin * win)
{
	cwin_fill(win, ' ', 1);
}

void cwin_fill(CharWin * win, char ch, char color)
{
	char	*sp = win->sp, * cp = win->cp;
	for(char y=0; y<win->wy; y++)
	{
		fill_fwd(sp, cp, ch, color, win->wx);
		sp += 40;
		cp += 40;
	}
}


void cwin_cursor_show(CharWin * win, bool show)
{
	char * cp = win->sp + mul40[win->cy] + win->cx;
	if (show)
		*cp |= 0x80;
	else
		*cp &= 0x7f;
}

void cwin_cursor_move(CharWin * win, char cx, char cy)
{
	win->cx = cx;
	win->cy = cy;
}


bool cwin_cursor_left(CharWin * win)
{
	if (win->cx > 0)
	{
		win->cx--;
		return true;
	}

	return false;
}

bool cwin_cursor_right(CharWin * win)
{
	if (win->cx + 1 < win->wx)
	{		
		win->cx++;
		return true;
	}

	return false;
}

bool cwin_cursor_up(CharWin * win)
{
	if (win->cy > 0)
	{
		win->cy--;
		return true;
	}

	return false;
}

bool cwin_cursor_down(CharWin * win)
{
	if (win->cy + 1 < win->wy)
	{		
		win->cy++;
		return true;
	}

	return false;
}

bool cwin_cursor_newline(CharWin * win)
{
	win->cx = 0;
	if (win->cy + 1 < win->wy)
	{		
		win->cy++;
		return true;
	}

	return false;	
}

bool cwin_cursor_forward(CharWin * win)
{
	if (win->cx + 1 < win->wx)
	{
		win->cx++;
		return true;
	}
	else if (win->cy + 1 < win->wy)
	{
		win->cx = 0;
		win->cy++;
		return true;
	}

	return false;
}

bool cwin_cursor_backward(CharWin * win)
{
	if (win->cx > 0)
	{
		win->cx--;
		return true;
	}
	else if (win->cy > 0)
	{
		win->cx = win->wx - 1;
		win->cy--;		
		return true;			
	}

	return false;
}

//static char p2smap[] = {0x00, 0x20, 0x00, 0x40, 0x00, 0x60, 0x40, 0x60};
static char p2smap[] = {0x00, 0x00, 0x40, 0x20, 0x80, 0xc0, 0x80, 0x80};
//static char s2pmap[] = {0x40, 0x20, 0x60, 0xa0, 0x40, 0x20, 0x60, 0xa0};
static char s2pmap[] = {0x40, 0x00, 0x20, 0xc0, 0xc0, 0x80, 0xa0, 0x40};

static inline char p2s(char ch)
{
	return ch ^ p2smap[ch >> 5];
}

static inline char s2p(char ch)
{
	return ch ^ s2pmap[ch >> 5];
}

void cwin_read_string(CharWin * win, char * buffer)
{
	char * sp = win->sp;

	char i = 0;
	for(char y=0; y<win->wy; y++)
	{
		for(char x=0; x<win->wx; x++)
		{
			buffer[i++] = s2p(sp[x]);
		}
		sp += 40;
	}	
	while (i > 0 && buffer[i - 1] == ' ')
		i--;
	buffer[i] = 0;
}

void cwin_write_string(CharWin * win, const char * buffer)
{
	char * dp = win->sp;
	for(char y=0; y<win->wy; y++)
	{
		for(char x=0; x<win->wx; x++)
		{
			char ch = *buffer;
			if (ch)
			{
				dp[x] = p2s(ch);
				buffer++;
			}
			else
				dp[x] = ' ';
		}
		dp += 40;
	}	

}
void cwin_put_char(CharWin * win, char ch, char color)
{
	cwin_putat_char(win, win->cx, win->cy, ch, color);
	win->cx++;
	if (win->cx == win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
}

void cwin_put_chars(CharWin * win, const char * chars, char num, char color)
{
	cwin_putat_chars(win, win->cx, win->cy, chars, color);
	win->cx += num;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
}

char cwin_put_string(CharWin * win, const char * str, char color)
{
	char n = cwin_putat_string(win, win->cx, win->cy, str, color);
	win->cx += n;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
	return n;
}


void cwin_put_char_raw(CharWin * win, char ch, char color)
{
	cwin_putat_char_raw(win, win->cx, win->cy, ch, color);
	win->cx++;
	if (win->cx == win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
}

void cwin_put_chars_raw(CharWin * win, const char * chars, char num, char color)
{
	cwin_putat_chars_raw(win, win->cx, win->cy, chars, color);
	win->cx += num;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
}

char cwin_put_string_raw(CharWin * win, const char * str, char color)
{
	char n = cwin_putat_string_raw(win, win->cx, win->cy, str, color);
	win->cx += n;
	if (win->cx >= win->wx)
	{
		win->cx = 0;
		win->cy++;
	}
	return n;	
}



void cwin_putat_char(CharWin * win, char x, char y, char ch, char color)
{
	int	offset = mul40[y] + x;

	win->sp[offset] = p2s(ch);
	win->cp[offset] = color;
}

#pragma native(cwin_putat_char)

void cwin_putat_chars(CharWin * win, char x, char y, const char * chars, char num, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;

	for(char i=0; i<num; i++)
	{
		char	ch = chars[i];

		sp[i] = p2s(ch);
		cp[i] = color;
	}
}

#pragma native(cwin_putat_chars)

char cwin_putat_string(CharWin * win, char x, char y, const char * str, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;
	
	char	i = 0;
	while (char	ch = str[i])
	{
		sp[i] = p2s(ch);
		cp[i] = color;
		i++;
	}

	return i;
}

#pragma native(cwin_putat_string)

void cwin_putat_char_raw(CharWin * win, char x, char y, char ch, char color)
{
	int	offset = mul40[y] + x;

	win->sp[offset] = ch;
	win->cp[offset] = color;
}

#pragma native(cwin_putat_char_raw)

void cwin_putat_chars_raw(CharWin * win, char x, char y, const char * chars, char num, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;

	for(char i=0; i<num; i++)
	{
		char	ch = chars[i];

		sp[i] = ch;
		cp[i] = color;
	}
}

#pragma native(cwin_putat_chars_raw)

char cwin_putat_string_raw(CharWin * win, char x, char y, const char * str, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;
	
	char	i = 0;
	while (char	ch = str[i])
	{
		sp[i] = ch;
		cp[i] = color;
		i++;
	}

	return i;
}

#pragma native(cwin_putat_string_raw)

char cwin_getat_char(CharWin * win, char x, char y)
{
	char * sp = win->sp + mul40[y] + x;

	return s2p(*sp);
}

#pragma native(cwin_getat_char)

void cwin_getat_chars(CharWin * win, char x, char y, char * chars, char num)
{
	char * sp = win->sp + mul40[y] + x;

	for(char i=0; i<num; i++)
	{
		chars[i] = s2p(sp[i]);
	}
}

#pragma native(cwin_getat_chars)

char cwin_getat_char_raw(CharWin * win, char x, char y)
{
	char * sp = win->sp + mul40[y] + x;

	return *sp;
}

#pragma native(cwin_getat_chars_raw)

void cwin_getat_chars_raw(CharWin * win, char x, char y, char * chars, char num)
{
	char * sp = win->sp + mul40[y] + x;

	for(char i=0; i<num; i++)
	{
		chars[i] = sp[i];
	}
}


#pragma native(cwin_put_rect_raw)

void cwin_put_rect_raw(CharWin * win, char x, char y, char w, char h, const char * chars, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;

	for(char i=0; i<h; i++)
	{

		for(char j=0; j<w; j++)
		{
			sp[j] = chars[j];
			cp[j] = color;
		}

		chars += w;
		sp += 40;
		cp += 40;
	}
}

#pragma native(cwin_put_rect)

void cwin_put_rect(CharWin * win, char x, char y, char w, char h, const char * chars, char color)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;

	for(char i=0; i<h; i++)
	{

		for(char j=0; j<w; j++)
		{
			sp[j] = p2s(chars[j]);
			cp[j] = color;
		}

		chars += w;
		sp += 40;
		cp += 40;
	}
}

#pragma native(cwin_get_rect_raw)

void cwin_get_rect_raw(CharWin * win, char x, char y, char w, char h, char * chars)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;

	for(char i=0; i<h; i++)
	{

		for(char j=0; j<w; j++)
		{
			chars[j] = sp[j];
		}

		chars += w;
		sp += 40;
	}
}

#pragma native(cwin_get_rect)

void cwin_get_rect(CharWin * win, char x, char y, char w, char h, char * chars)
{
	int	offset = mul40[y] + x;

	char	*	sp = win->sp + offset;

	for(char i=0; i<h; i++)
	{

		for(char j=0; j<w; j++)
		{
			chars[j] = s2p(sp[j]);
		}

		chars += w;
		sp += 40;
	}
}


#pragma native(cwin_getat_chars_raw)

void cwin_insert_char_raw(CharWin * win, char ch, char color)
{
	char y = win->wy - 1, rx = win->wx - 1;

	char * sp = win->sp + mul40[y];
	char * cp = win->cp + mul40[y];
	
	while (y > win->cy)
	{		
		copy_bwd(sp + 1, sp, cp + 1, cp, rx);

		sp -= 40;
		cp -= 40;
		sp[40] = sp[rx];
		cp[40] = cp[rx];
		y--;
	}

	sp += win->cx;
	cp += win->cx;
	rx -= win->cx;

	copy_bwd(sp + 1, sp, cp + 1, cp, rx);

	sp[0] = ch;
	cp[0] = color;
}

void cwin_insert_char(CharWin * win, char ch, char color)
{
	cwin_insert_char_raw(win, p2s(ch), color);
}

void cwin_delete_char(CharWin * win)
{
	char * sp = win->sp + mul40[win->cy];
	char * cp = win->cp + mul40[win->cy];
	
	char x = win->cx, rx = win->wx - 1;

	copy_fwd(sp + x, sp + x + 1, cp + x, cp + x + 1, rx - x);

	char y = win->cy + 1;
	while (y < win->wy)
	{
		sp[rx] = sp[40];
		cp[rx] = cp[40];

		sp += 40;
		cp += 40;

		copy_fwd(sp, sp + 1, cp, cp + 1, rx);

		y++;
	}

	sp[rx] = ' ';
}

int cwin_getch(void)
{
	__asm
	{
		L1:
			jsr	0xffe4
			cmp	#0
			beq	L1
			sta	accu
			lda	#0
			sta	accu + 1
	}
}

int cwin_checkch(void)
{
	__asm
	{
		L1:
			jsr	0xffe4
			sta	accu
			lda	#0
			sta	accu + 1
	}
}

bool cwin_edit_char(CharWin * win, char ch)
{
	switch (ch)
	{
	case 13:
	case 3:
		return true;
	
	case 19:
		win->cx = 0;
		win->cy = 0;
		return false;
		
	case 147:
		cwin_clear(win);
		win->cx = 0;
		win->cy = 0;
		return false;
	
	case 17:
		cwin_cursor_down(win);
		return false;

	case 145: // CRSR_UP
		cwin_cursor_up(win);
		return false;

	case 29:
		cwin_cursor_forward(win);
		return false;

	case 157:
		cwin_cursor_backward(win);
		return false;

	case 20:
		if (cwin_cursor_backward(win))
			cwin_delete_char(win);
		return false;

	default:
		if (ch >= 32 && ch < 128 || ch >= 160)
		{
			if (win->cy + 1 < win->wy || win->cx + 1 < win->wx)
			{
				cwin_insert_char(win, ch, 1);
				cwin_cursor_forward(win);
			}
		}
		return false;
	}
}

char cwin_edit(CharWin * win)
{
	for(;;)
	{
		cwin_cursor_show(win, true);
		char ch = cwin_getch();
		cwin_cursor_show(win, false);

		if (cwin_edit_char(win, ch))
			return ch;
	}
}

void cwin_scroll_left(CharWin * win, char by)
{
	char * sp = win->sp;
	char * cp = win->cp;
	
	char rx = win->wx - by;

	for(char y=0; y<win->wy; y++)
	{
		copy_fwd(sp, sp + by, cp, cp + by, rx);
	}
}

void cwin_scroll_right(CharWin * win, char by)
{
	char * sp = win->sp;
	char * cp = win->cp;
	
	char rx = win->wx - by;

	for(char y=0; y<win->wy; y++)
	{
		copy_bwd(sp + by, sp, cp + by, cp, rx);
		sp += 40;
		cp += 40;
	}
}

void cwin_scroll_up(CharWin * win, char by)
{
	char * sp = win->sp;
	char * cp = win->cp;
	
	char rx = win->wx;
	int	dst = mul40[by];

	for(char y=0; y<win->wy - by; y++)
	{
		copy_fwd(sp, sp + dst, cp, cp + dst, rx);
		sp += 40;
		cp += 40;
	}	
}

void cwin_scroll_down(CharWin * win, char by)
{
	char * sp = win->sp + mul40[win->wy];
	char * cp = win->cp + mul40[win->wy];
	
	char rx = win->wx;

	int	dst = mul40[by];

	for(char y=0; y<win->wy - by; y++)
	{
		sp -= 40;
		cp -= 40;
		copy_bwd(sp, sp - dst, cp, cp - dst, rx);
	}	
}

void cwin_fill_rect_raw(CharWin * win, char x, char y, char w, char h, char ch, char color)
{
	if (w > 0)
	{
		char * sp = win->sp + mul40[y] + x;
		char * cp = win->cp + mul40[y] + x;

		for(char y=0; y<h; y++)
		{
			fill_fwd(sp, cp, ch, color, w);
			sp += 40;
			cp += 40;		
		}
	}
}

void cwin_fill_rect(CharWin * win, char x, char y, char w, char h, char ch, char color)
{
	cwin_fill_rect_raw(win, x, y, w, h, p2s(ch), color);
}

void cwin_console_scroll_up(CharWin * win)
{
	win->cy--;
	win->ly--;
	cwin_scroll_up(win, 1);
	cwin_fill_rect(win, 0, win->wy - 1, win->wx, 1, ' ', 1);	
}

void cwin_console_newline(CharWin * win)
{
	win->cx = 0;
	win->cy++;
	if (win->cy == win->wy)
		cwin_console_scroll_up(win);
}

void cwin_console_write_char(CharWin * win, char ch, char color)
{
	if (win->cx == win->wx)
		cwin_console_newline(win);

	int	offset = mul40[win->cy] + win->cx;

	win->sp[offset] = p2s(ch);
	win->cp[offset] = color;
	win->cx++;
}

void cwin_console_write_string(CharWin * win, const char * chars, char color)
{
	win->ly = win->cy;
	win->lx = win->cx;

	char i = 0;
	while (char ch = chars[i])
	{
		if (ch == '\n')
			cwin_console_newline(win);
		else
			cwin_console_write_char(win, ch, color);
		i++;
	}
}

void cwin_console_clear(CharWin * win)
{
	cwin_fill_rect(win, win->lx, win->ly, win->wx - win->lx, 1, ' ', 1);
	cwin_fill_rect(win, 0, win->ly + 1, win->wx, win->wy - win->ly - 1, ' ', 1);
}

bool cwin_console_cursor_left(CharWin * win)
{
	if (win->cy == win->ly)
	{
		if (win->cx > win->lx)
		{
			win->cx--;
			return true;
		}
	}
	else if (win->cx > 0)
	{
		win->cx--;
		return true;
	}
	else
	{
		win->cy--;
		win->cx = win->wx - 1;
		return true;
	}
	return false;
}

bool cwin_console_cursor_right(CharWin * win)
{
	if (win->cx + 1 < win->wx)
	{
		win->cx++;
		return true;
	}
	else if (win->cy + 1 < win->wy)
	{
		win->cy++;
		win->cx = 0;
		return true;
	}
	else if (win->ly > 0)
	{
		win->cx = 0;
		win->cy++;
		cwin_console_scroll_up(win);
		return true;
	}

	return false;
}

void cwin_console_delete_char(CharWin * win)
{
	cwin_delete_char(win);
}

bool cwin_console_insert_char(CharWin * win, char ch, char color)
{
	if (win->sp[mul40[win->wy - 1] + win->wx - 1] != ' ')
	{
		if (win->ly == 0)
			return false;
		cwin_console_scroll_up(win);
	}

	cwin_insert_char(win, ch, color);
	return true;
}

bool cwin_console_edit_char(CharWin * win, char ch, char color)
{
	switch (ch)
	{
	case 13:
	case 3:
	case 17:
	case 145: // CRSR_UP
		return true;
	
	case 19:
		win->cx = win->lx;
		win->cy = win->ly;
		return false;
		
	case 147:		
		cwin_console_clear(win);
		win->cx = win->lx;;
		win->cy = win->ly;
		return false;
	
	case 29:
		cwin_console_cursor_right(win);
		return false;

	case 157:
		cwin_console_cursor_left(win);
		return false;

	case 20:
		if (cwin_console_cursor_left(win))
			cwin_console_delete_char(win);
		return false;

	default:
		if (ch >= 32 && ch < 128 || ch >= 160)
		{
			if (cwin_console_insert_char(win, ch, color))
				cwin_console_cursor_right(win);
		}
		return false;
	}
}

char cwin_console_edit_string(CharWin * win, char color)
{
	for(;;)
	{
		cwin_cursor_show(win, true);
		char ch = cwin_getch();
		cwin_cursor_show(win, false);

		if (cwin_console_edit_char(win, ch, color))
		{
			win->cx = win->lx;
			win->cy = win->ly;
			return ch;
		}
	}	
}

void cwin_console_get_string(CharWin * win, char * chars, char size)
{
	char i = 0;
	char y = win->ly, x = win->lx;
	char * cp = win->sp + mul40[y];

	while (i < size)
	{
		chars[i++] = s2p(cp[x++]);
		if (x == win->wx)
		{
			if (y + 1 == win->wy)
				break;
			x = 0;
			cp += 40;
			y++;
		}
	}

	while (i > 0 && chars[i - 1] == ' ')
	{
		i--;
		if (x == 0)
		{
			y--;
			x = win->wx;
		}
		else
			x--;
	}

	win->cx = x;
	win->cy = y;

	chars[i] = 0;	
}

char * sformat(char * buff, const char * fmt, int * fps, bool print);

void cwin_console_printf(CharWin * win, char color, const char * fmt, ...)
{
	char	buff[200];
	sformat(buff, fmt, (int *)&fmt + 1, false);
	cwin_console_write_string(win, buff, color);
}
