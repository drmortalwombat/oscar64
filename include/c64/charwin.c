#include "charwin.h"



static void copy_fwd(char * sdp, const char * ssp, char * cdp, const char * csp, char n)
{
	for(char i=0; i<n; i++)
	{
		sdp[i] = ssp[i];
		cdp[i] = csp[i];
	}
}

#pragma native(copy_fwd)

static void fill_fwd(char * sdp, char * cdp, char ch, char color, char n)
{
	for(char i=0; i<n; i++)
	{
		sdp[i] = ch;
		cdp[i] = color;
	}
}

#pragma native(fill_fwd)

static void copy_bwd(char * sdp, const char * ssp, char * cdp, const char * csp, char n)
{
	while (n)
	{
		n--;
		sdp[n] = ssp[n];
		cdp[n] = csp[n];
	}
}

#pragma native(copy_bwd)



void cwin_init(CharWin * win, char * screen, char sx, char sy, char wx, char wy)
{
	win->sx = sx;
	win->sy = sy;
	win->wx = wx;
	win->wy = wy;
	win->cx = 0;
	win->cy = 0;
	win->sp = screen + 40 * sy + sx;
	win->cp = (char *)0xd800 + 40 * sy + sx;
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
	char * cp = win->sp + 40 * win->cy + win->cx;
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

void cwin_read_string(CharWin * win, char * buffer)
{
	char * sp = win->sp;

	char i = 0;
	for(char y=0; y<win->wy; y++)
	{
		for(char x=0; x<win->wx; x++)
		{
			char c = sp[x];
			if (c & 0x40)
				c ^= 0xc0;
			if (!(c & 0x20))
				c |= 0x40;
			buffer[i++] = c;
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
			char c = *buffer;
			if (c)
			{
				c &= 0xbf;
				if (c & 0x80)
					c ^= 0xc0;
				
				dp[x] = c;
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

void cwin_putat_char(CharWin * win, char x, char y, char ch, char color)
{
	int	offset = 40 * y + x;

	ch &= 0xbf;
	if (ch & 0x80)
		ch ^= 0xc0;

	win->sp[offset] = ch;
	win->cp[offset] = color;
}

#pragma native(cwin_putat_char)

void cwin_putat_chars(CharWin * win, char x, char y, const char * chars, char num, char color)
{
	int	offset = 40 * y + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;

	for(char i=0; i<num; i++)
	{
		char	ch = chars[i];

		ch &= 0xbf;
		if (ch & 0x80)
			ch ^= 0xc0;

		sp[i] = ch;
		cp[i] = color;
	}
}

#pragma native(cwin_putat_chars)

char cwin_putat_string(CharWin * win, char x, char y, const char * str, char color)
{
	int	offset = 40 * y + x;

	char	*	sp = win->sp + offset;
	char	*	cp = win->cp + offset;
	
	char	i = 0;
	while (char	ch = str[i])
	{
		ch &= 0xbf;
		if (ch & 0x80)
			ch ^= 0xc0;

		sp[i] = ch;
		cp[i] = color;
		i++;
	}

	return i;
}

#pragma native(cwin_putat_string)


char cwin_getat_char(CharWin * win, char x, char y)
{
	char * sp = win->sp + 40 * y + x;

	char c = *sp;

	if (c & 0x40)
		c ^= 0xc0;
	if (!(c & 0x20))
		c |= 0x40;
	
	return c;
}

void cwin_getat_chars(CharWin * win, char x, char y, char * chars, char num)
{
	char * sp = win->sp + 40 * y + x;

	for(char i=0; i<num; i++)
	{
		char c = sp[i];

		if (c & 0x40)
			c ^= 0xc0;
		if (!(c & 0x20))
			c |= 0x40;
		
		chars[i] = c;
	}
}


void cwin_insert_char(CharWin * win)
{
	char y = win->wy - 1, rx = win->wx - 1;

	char * sp = win->sp + 40 * y;
	char * cp = win->cp + 40 * y;
	
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

	sp[0] = ' ';
}

void cwin_delete_char(CharWin * win)
{
	char * sp = win->sp + 40 * win->cy;
	char * cp = win->cp + 40 * win->cy;
	
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
				cwin_insert_char(win);
				cwin_put_char(win, ch, 1);
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
	int	dst = 40 * by;

	for(char y=0; y<win->wy - by; y++)
	{
		copy_fwd(sp, sp + dst, cp, cp + dst, rx);
		sp += 40;
		cp += 40;
	}	
}

void cwin_scroll_down(CharWin * win, char by)
{
	char * sp = win->sp + 40 * win->wy;
	char * cp = win->cp + 40 * win->wy;
	
	char rx = win->wx;

	int	dst = 40 * by;

	for(char y=0; y<win->wy - by; y++)
	{
		sp -= 40;
		cp -= 40;
		copy_fwd(sp, sp - dst, cp, cp - dst, rx);
	}	
}


void cwin_fill_rect(CharWin * win, char x, char y, char w, char h, char ch, char color)
{
	if (w > 0)
	{
		char * sp = win->sp + 40 * y + x;
		char * cp = win->cp + 40 * y + x;

		for(char y=0; y<h; y++)
		{
			fill_fwd(sp, cp, ch, color, w);
			sp += 40;
			cp += 40;		
		}
	}
}
