#include <assert.h>

#define Screen ((char *)0x0400)

void scroll_left(void)
{
	char	*	dp = Screen;
	for(char y=0; y<25; y++)
	{
		for(char x=0; x<39; x++)
		{
			dp[x] = dp[x + 1];
		}
		dp += 40;
	}
}

void scroll_right(void)
{
	char	*	dp = Screen;
	for(char y=0; y<25; y++)
	{
		for(char x=39; x>0; x--)
		{
			dp[x] = dp[x - 1];
		}
		dp += 40;
	}
}

void scroll_up(void)
{
	char	*	dp = Screen, * sp = dp + 40;
	for(char y=0; y<24; y++)
	{
		for(char x=0; x<40; x++)
		{
			dp[x] = sp[x];
		}
		dp = sp;
		sp += 40;
	}
}

void scroll_down(void)
{
	char	*	dp = Screen + 24 * 40, * sp = dp - 40;
	for(char y=0; y<24; y++)
	{
		for(char x=0; x<40; x++)
		{
			dp[x] = sp[x];
		}
		dp = sp;
		sp -= 40;
	}
}

void fill_screen(void)
{
	for(char y=0; y<25; y++)
	{
		for(char x=0; x<40; x++)
		{
			Screen[40 * y + x] = 7 * y + x;
		}
	}
}

void check_screen(int dy, int dx)
{
	for(int y=0; y<25; y++)
	{
		for(int x=0; x<40; x++)
		{
			int sy = y + dy;
			int sx = x + dx;

			char	c = 7 * y + x;
			if (sy >= 0 && sy < 25 && sx >= 0 && sx < 40)
				c = 7 * sy + sx;

			assert(Screen[40 * y + x] == c);
		}
	}
}

int main(void)
{
	fill_screen();
	scroll_left();
	check_screen(0, 1);

	fill_screen();
	scroll_right();
	check_screen(0, -1);

	fill_screen();
	scroll_up();
	check_screen(1, 0);

	fill_screen();
	scroll_down();
	check_screen(-1, 0);

	return 0;
}
