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

// Screen and color space
#define screen ((byte *)0x0400)
#define color ((byte *)0xd800)

// Macro for easy access to screen space
#define sline(x, y) (screen + 40 * (y) + (x))

// Start row for text
#define srow 5

// Move the screen one character to the left
void scrollLeft(void)
{
	// Loop horizontaly
	for(char x=0; x<39; x++)
	{
		// Unroll vetical loop 16 times
#assign y 0		
#repeat
		sline(0, srow + y)[x] = sline(1, srow + y)[x];
#assign y y + 1
#until y == 16
	}
}

// Expand one column of a glyph to the right most screen column
void expand(char c, byte f)
{
	// Address of glyph data
	byte * fp = font + 8 * c;

	// Unroll eight times for each byte in glyph data
#assign y 0		
#repeat
	sline(39, srow + 2 * y + 0)[0] = 
	sline(39, srow + 2 * y + 1)[0] = (fp[y] & f) ? 160 : 32;
#assign y y + 1
#until y == 8

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
		memset(color + 40 * (srow + i), i + 1, 40);

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
