#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>

byte font[2048];

// Copy the system font into local RAM for easy access
void copyFont(void)
{
	mmap_set(MMAP_CHAR_ROM);

	memcpy(font, (byte *)0xd800, 2048);

	mmap_set(MMAP_ROM);
}

// Single row of screen has 40 characters
typedef char	ScreenRow[40];

// Screen and color space
ScreenRow * const screen = (ScreenRow *)0x0400;
ScreenRow * const color = (ScreenRow *)0xd800;

// Start row for text
#define srow 5

// Move the screen one character to the left
void scrollLeft(void)
{
	// Loop horizontaly
	for(char x=0; x<39; x++)
	{
		// Unroll vertical loop 16 times
		#pragma unroll(full)
		for(char y=0; y<16; y++)
		{
			screen[srow + y][x] = screen[srow + y][x + 1];
		}
	}
}

// Expand one column of a glyph to the right most screen column
void expand(char c, byte f)
{
	// Address of glyph data
	byte * fp = font + 8 * c;

	// Unroll eight times for each byte in glyph data
//	#pragma unroll(full)
	for(char y=0; y<8; y++)
	{
		char t = (fp[y] & f) ? 160 : 32;
		screen[srow + 2 * y + 0][39] = t;
		screen[srow + 2 * y + 1][39] = t;
	}
}

const char * text = 
	s"Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt "
	s"ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
	s"dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit "
	s"amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor "
	s"invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam "
	s"et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet."


int main(void)
{
	// Install the IRQ trampoline
	mmap_trampoline();

	// Copy the font data
	copyFont();

	// Cleat the screen
	memset(screen, 0x20, 1000);

	// Color bars
	for(int i=0; i<16; i++)
		memset(color[srow + i], i + 1, 40);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	// Hide left and right column
	vic.ctrl2 = 0;

	// Loop over text
	int	ci = 0;
	for(;;)
	{
		// Loop over glyph from left to right
		byte cf = 0x80;
		while (cf)
		{
			for(char i=0; i<2; i++)
			{
				// Pixel level scrolling
				vic_waitBottom();
				vic.ctrl2 = 4;
				vic_waitTop();

				vic_waitBottom();
				vic.ctrl2 = 2;
				vic_waitTop();

				vic_waitBottom();
				vic.ctrl2 = 0;
				vic_waitTop();

				vic_waitBottom();
				vic.ctrl2 = 6;

				// Crossing character border, now scroll and show new column
				scrollLeft();
				expand(text[ci], cf);
			}

			// Next glyph column
			cf >>= 1;
		}

		// Next character
		ci++;
	}

	return 0;
}
