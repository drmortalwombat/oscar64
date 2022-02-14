#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sid.h>
#include <c64/charwin.h>
#include <c64/types.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Include charset and sprite resources

char charset[2048] = {
	#embed "../resources/connect4chars.bin"
};

char spriteset[2048] = {
	#embed "../resources/connect4sprites.bin"
};

// Address ov video data

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xd000;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd800;

// Values for player 1 pieces and player 2 pieces.
// The values 1 and 5 are selected so that the sum of the
// values in a row of four will add up to a unique value
// in the range of 0..24 for ranking

#define PLAYER1_INC		1
#define PLAYER2_INC		5

// Current state of the board

struct Board
{	
	char	fcols[48];	// Pieces in the 6x7 grid using 48 to have a multplier of 8
	char	ffree[7];	// Number of free places in each column
}	board;

enum GameState
{
	GS_READY,			// Getting ready

	GS_PLAYER_MOVE,		// The players move
	GS_COMPUTER_MOVE,	// The computers move

	GS_PLAYER_WIN,		// The player wins
	GS_COMPUTER_WIN,	// The computer wins
	GS_GAME_OVER		// No more moves
};

// Draw the board into the CharWin using the small
// character set

void board_draw(CharWin * cwin)
{
	for(char y=0; y<6; y++)
	{
		for(char x=0; x<7; x++)
		{
			if (board.fcols[8 * y + x] == PLAYER1_INC)
				cwin_putat_char_raw(cwin, x, y, 82, 10);
			else if (board.fcols[8 * y + x] == PLAYER2_INC)
				cwin_putat_char_raw(cwin, x, y, 82, 15);
			else
				cwin_putat_char_raw(cwin, x, y, 82,  8);
		}

		// Right most column

		cwin_putat_char_raw(cwin, 7, y, 83, 8);
	}
}

// Draw a big piece or place with the given color, the
// color 0 denotes an empty place.  Empty places use different
// tiles to allow sprite priority to obscure the falling pieces

void board_draw_item(CharWin * cwin, char x, char y, char c)
{
	char cx = x * 3 + 1, cy = y * 3 + 2;

	if (c)
	{
		cwin_putat_char_raw(cwin, cx + 0, cy + 0, 73, c);
		cwin_putat_char_raw(cwin, cx + 1, cy + 0, 74, c);
		cwin_putat_char_raw(cwin, cx + 2, cy + 0, 75, c);

		cwin_putat_char_raw(cwin, cx + 0, cy + 1, 76, c);
		cwin_putat_char_raw(cwin, cx + 1, cy + 1, 77, c);
		cwin_putat_char_raw(cwin, cx + 2, cy + 1, 78, c);

		cwin_putat_char_raw(cwin, cx + 0, cy + 2, 79, c);
		cwin_putat_char_raw(cwin, cx + 1, cy + 2, 80, c);
		cwin_putat_char_raw(cwin, cx + 2, cy + 2, 81, c);
	}
	else
	{
		cwin_putat_char_raw(cwin, cx + 0, cy + 0, 64, 14);
		cwin_putat_char_raw(cwin, cx + 1, cy + 0, 65, 14);
		cwin_putat_char_raw(cwin, cx + 2, cy + 0, 66, 14);

		cwin_putat_char_raw(cwin, cx + 0, cy + 1, 67, 14);
		cwin_putat_char_raw(cwin, cx + 1, cy + 1, 68, 14);
		cwin_putat_char_raw(cwin, cx + 2, cy + 1, 69, 14);

		cwin_putat_char_raw(cwin, cx + 0, cy + 2, 70, 14);
		cwin_putat_char_raw(cwin, cx + 1, cy + 2, 71, 14);
		cwin_putat_char_raw(cwin, cx + 2, cy + 2, 72, 14);		
	}
}

// Draw the big board

void board_draw_main(CharWin * cwin)
{
	for(char y=0; y<6; y++)
	{
		for(char x=0; x<7; x++)
		{
			if (board.fcols[8 * y + x] == PLAYER1_INC)
				board_draw_item(cwin, x, y, 10);
			else if (board.fcols[8 * y + x] == PLAYER2_INC)
				board_draw_item(cwin, x, y, 15);
			else
				board_draw_item(cwin, x, y, 0);
		}
	}
}

// Draw the decoration of the big board

void board_init_main(CharWin * cwin)
{
	// Frame around the board

	cwin_putat_char_raw(cwin, 0, 1, 84, 14);
	cwin_fill_rect_raw(cwin, 1, 1, 21, 1, 85, 14);
	cwin_putat_char_raw(cwin, 22, 1, 86, 14);
	cwin_fill_rect_raw(cwin, 0, 2, 1, 18, 87, 14);
	cwin_fill_rect_raw(cwin, 22, 2, 1, 18, 88, 14);
	cwin_putat_char_raw(cwin, 0, 20, 89, 14);
	cwin_fill_rect_raw(cwin, 1, 20, 21, 1, 90, 14);
	cwin_putat_char_raw(cwin, 22, 20, 91, 14);

	// Fields in the board

	for(char x=0; x<7; x++)
		cwin_putat_char_raw(cwin, 3 * x + 2, 0, '1' + x, 1);

	board_draw_main(cwin);
}

// Play animation of a dropped piece

void item_drop_anim(CharWin * cwin, char x, char y, char c)
{
	// Initial position

	int	ix = (cwin->sx + 3 * x + 1) * 8 + 24, iy = (cwin->sy + 2) * 8 + 20;

	// Show sprite at start position
	spr_set(0, true, ix, iy, 97,  c, true, false, false);

	// Set sprite priority to be behind background
	vic.spr_priority = 0x01;

	// Speed and target position, using four fractional bits
	int	vy = 0, aiy = iy * 16, ty = ((cwin->sy + y * 3 + 2) * 8 + 50) * 16;

	// Bounce three times back when reaching bottom
	char	bounce = 3;

	// Loop through the bounces
	while (bounce > 0)
	{
		// Let gravity do its thing of accelerating
		vy += 3;

		// Add vertical velocity to position
		aiy += vy;

		// Reached the bottom yet?
		if (aiy > ty)
		{
			// Reflect position
			aiy = 2 * ty - aiy;

			// Reflect speed
			vy = - vy * 5 >> 4;

			// One bounce down
			bounce--;
		}

		// Move sprite and wait for display
		spr_move(0, ix, aiy >> 4);
		vic_waitFrame();
	}
}

// Play column selection animation

void column_select_anim(CharWin * cwin, char x0, char x1, char c)
{
	// Start position
	int	ix = (cwin->sx + 3 * x0 + 1) * 8 + 24, iy = (cwin->sy + 2) * 8 + 20;

	// Target position
	int	tx = (cwin->sx + 3 * x1 + 1) * 8 + 24;

	// Show sprite at start position
	spr_set(0, true, ix, iy, 97,  c, true, false, false);

	// Set sprite priority to be behind background
	vic.spr_priority = 0x01;

	// Not at target position already?
	if (ix != tx)
	{
		// Eight frames for movement
		for(int i=1; i<=8; i++)
		{
			vic_waitFrame();
			// Move sprite along path
			spr_move(0, (ix * (8 - i) + tx * i) >> 3, iy);
		}
	}
}

// Initialize the board to empty

void board_init(void)
{
	// Column has six free slots
	for(char x=0; x<7; x++)
		board.ffree[x] = 6;

	// Clear all slots
	for(char i=0; i<48; i++)
		board.fcols[i] = 0;
}

// Drop one piece of player p down column x

inline bool board_drop(char x, bool p)
{
	// Check for free space
	if (board.ffree[x])
	{
		// Fill one slot
		char	y = --(board.ffree[x]);
		board.fcols[8 * y + x] = p ? PLAYER2_INC : PLAYER1_INC;

		// Success
		return true;
	}
	return false;
}

// Remove the top piece from column x

inline void board_undrop(char x)
{
	// Free one slot
	char	y = (board.ffree[x])++;
	board.fcols[8 * y + x] = 0x00;
}

// Score for rows with 0, 1, 2, and 3 elements of same color

static const int score[5] = {0, 1, 8, 128, 10000};

// Score index is calculated (25 * rv + 5 * p1 + p2) with rv the
// value range of the row, p1 and p2 number of pieces from player 1 or 2

int fscore[125];

// Indices of the board positions of four slots of each of the 69 
// 4 slot rows in the board.  The fifth value is the value range
// of the row

char frows[5][69];

#pragma align(fscore, 256)

// Tables for opening libary for computer move 1, 2, 3 and 4

const char open1 = 3;
const char open2[4 * 7] = 
{
	4, 3, 3, 3, 4, 3, 3, 3, 5, 3, 3, 4, 3, 3, 3, 3, 2, 3, 2, 2, 2, 4, 2, 4, 3, 2, 4, 2
};

const char open3[4 * 7 * 7] = {
	0,  5,  2,  2,  5,  3,  2,  5,  4,  2,  4,  3,  3,  5,  2,  2,  3,  2,  3,  2,  2,  4,  5,  2,  4,  4,  3,  2,
	3,  3,  3,  3,  3,  3,  4,  1,  3,  2,  5,  3,  2,  1,  2,  5,  2,  2,  4,  1,  4,  5,  4,  2,  4,  3,  3,  5,
	4,  1,  3,  4,  4,  4,  3,  2,  1,  3,  2,  3,  2,  3,  5,  5,  1,  5,  4,  3,  3,  3,  4,  3,  3,  5,  3,  3,
	3,  5,  2,  3,  4,  1,  3,  5,  4,  3,  1,  4,  3,  5,  2,  2,  3,  2,  3,  2,  2,  2,  1,  3,  2,  3,  2,  3,
	3,  1,  3,  2,  2,  3,  3,  2,  1,  3,  2,  2,  2,  2,  3,  3,  3,  2,  1,  3,  3,  3,  3,  1,  3,  3,  2,  3,
	2,  3,  3,  3,  3,  3,  3,  3,  2,  1,  2,  3,  4,  3,  2,  4,  3,  2,  2,  2,  2,  1,  0,  2,  4,  3,  4,  4,
	4,  4,  1,  2,  5,  2,  2,  2,  2,  3,  2,  4,  6,  5,  4,  4,  4,  4,  3,  2,  4,  3,  2,  3,  4,  5,  4,  3,
};

const char open4[4 * 7 * 7 * 7] = {
	2,  5,  2,  2,  5,  3,  2,  2,  2,  6,  2,  2,  2,  2,  0,  4,  4,  3,  2,  3,  3,  5,  5,  5,  5,  5,  1,  1,
	2,  2,  6,  2,  2,  2,  2,  0,  4,  4,  4,  3,  5,  4,  1,  5,  1,  1,  1,  1,  1,  0,  4,  2,  4,  3,  4,  5,
	5,  1,  4,  5,  5,  3,  2,  3,  3,  1,  1,  1,  1,  3,  5,  5,  4,  5,  5,  4,  4,  3,  3,  3,  1,  3,  3,  3,
	3,  3,  3,  1,  3,  3,  3,  5,  4,  2,  5,  3,  1,  4,  0,  3,  6,  2,  3,  3,  3,  3,  3,  1,  1,  1,  1,  3,
	3,  3,  3,  2,  3,  3,  3,  2,  1,  6,  2,  4,  5,  2,  3,  3,  3,  2,  3,  3,  3,  3,  1,  5,  5,  3,  3,  3,
	3,  3,  0,  2,  3,  3,  3,  0,  5,  2,  2,  5,  1,  2,  4,  4,  2,  4,  5,  4,  2,  2,  2,  3,  2,  2,  2,  2,
	2,  5,  5,  2,  2,  1,  2,  4,  3,  4,  4,  3,  3,  4,  1,  3,  2,  1,  4,  4,  1,  4,  5,  4,  4,  4,  2,  4,
	0,  3,  3,  4,  3,  3,  3,  3,  3,  3,  1,  3,  5,  3,  3,  3,  3,  2,  2,  4,  3,  4,  3,  4,  4,  3,  3,  4,
	3,  3,  2,  3,  3,  5,  6,  3,  5,  4,  5,  5,  3,  3,  3,  3,  2,  3,  3,  3,  5,  0,  5,  3,  4,  4,  3,  2,
	3,  3,  3,  1,  3,  3,  3,  3,  1,  5,  5,  3,  3,  3,  3,  3,  2,  1,  4,  3,  1,  3,  3,  3,  4,  3,  3,  3,
	2,  2,  3,  2,  5,  5,  4,  2,  5,  3,  1,  4,  2,  1,  0,  2,  4,  4,  3,  2,  4,  5,  4,  2,  5,  3,  1,  4,
	3,  3,  0,  2,  3,  3,  3,  4,  2,  4,  4,  2,  2,  4,  3,  3,  3,  4,  6,  3,  3,  2,  5,  3,  1,  4,  2,  1,
	2,  2,  3,  2,  2,  4,  6,  0,  4,  2,  4,  3,  4,  5,  5,  1,  4,  5,  5,  3,  2,  3,  3,  1,  1,  1,  1,  3,
	5,  5,  4,  5,  5,  4,  4,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  5,  4,  2,  5,  3,  1,  4,
	2,  2,  6,  2,  2,  2,  2,  4,  4,  3,  4,  4,  4,  3,  4,  1,  3,  5,  3,  3,  3,  2,  2,  6,  2,  2,  2,  2,
	5,  1,  3,  3,  5,  4,  3,  2,  2,  6,  2,  2,  2,  2,  4,  1,  3,  5,  3,  1,  3,  3,  3,  1,  1,  1,  1,  3,
	6,  2,  3,  1,  4,  5,  4,  3,  3,  3,  2,  3,  3,  3,  1,  2,  3,  1,  4,  1,  1,  3,  3,  3,  4,  3,  2,  3,
	1,  3,  1,  1,  4,  2,  1,  3,  3,  3,  2,  3,  3,  3,  4,  4,  2,  4,  5,  4,  2,  4,  1,  6,  4,  4,  4,  2,
	1,  3,  2,  4,  3,  1,  1,  4,  4,  4,  4,  4,  4,  2,  3,  3,  4,  4,  3,  3,  4,  3,  5,  3,  1,  3,  1,  3,
	5,  2,  2,  5,  4,  3,  5,  3,  3,  3,  1,  3,  5,  3,  3,  1,  4,  4,  5,  3,  3,  3,  1,  3,  2,  2,  2,  3,
	3,  3,  4,  4,  3,  3,  4,  3,  5,  3,  3,  5,  5,  3,  5,  4,  2,  5,  5,  3,  5,  3,  3,  3,  1,  1,  5,  3,
	3,  3,  3,  1,  3,  3,  3,  5,  1,  1,  3,  4,  3,  3,  1,  3,  1,  1,  4,  2,  1,  5,  5,  2,  1,  4,  1,  1,
	5,  4,  2,  5,  5,  3,  5,  3,  3,  2,  3,  5,  5,  1,  3,  3,  3,  5,  3,  3,  3,  5,  4,  2,  5,  3,  1,  4,
	2,  1,  1,  4,  3,  4,  4,  3,  3,  3,  2,  3,  3,  3,  5,  3,  2,  5,  4,  3,  3,  3,  3,  3,  1,  1,  5,  3,
	3,  3,  3,  5,  3,  3,  3,  4,  3,  2,  2,  3,  1,  6,  0,  3,  6,  2,  3,  3,  3,  3,  3,  1,  1,  1,  1,  3,
	3,  3,  3,  2,  3,  3,  3,  2,  1,  6,  2,  4,  5,  2,  3,  3,  3,  2,  3,  3,  3,  3,  1,  5,  5,  3,  3,  3,
	3,  3,  0,  2,  3,  3,  3,  3,  3,  1,  1,  1,  1,  3,  6,  2,  3,  1,  4,  5,  4,  3,  3,  3,  2,  3,  3,  3,
	1,  2,  3,  1,  4,  1,  1,  3,  3,  3,  4,  3,  2,  3,  1,  3,  1,  1,  4,  2,  1,  3,  3,  3,  2,  3,  3,  3,
	3,  3,  3,  4,  3,  3,  3,  2,  3,  1,  3,  0,  1,  1,  3,  3,  3,  3,  4,  3,  3,  3,  3,  3,  3,  2,  2,  3,
	4,  4,  4,  2,  2,  3,  4,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  3,  2,  2,  2,  2,
	1,  3,  2,  4,  3,  1,  1,  0,  1,  2,  2,  4,  5,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
	2,  3,  3,  2,  2,  3,  3,  2,  3,  3,  2,  2,  3,  2,  3,  1,  3,  4,  3,  3,  3,  1,  1,  4,  4,  2,  4,  2,
	3,  4,  3,  4,  3,  4,  4,  3,  2,  3,  2,  3,  3,  3,  1,  2,  2,  2,  4,  4,  2,  3,  4,  4,  4,  3,  5,  3,
	3,  2,  4,  4,  3,  3,  3,  3,  1,  5,  5,  3,  3,  3,  1,  3,  1,  1,  4,  2,  1,  3,  1,  1,  3,  3,  1,  3,
	2,  3,  3,  2,  2,  3,  3,  3,  4,  4,  4,  3,  5,  3,  3,  3,  1,  2,  2,  5,  3,  3,  1,  3,  5,  3,  3,  3,
	1,  3,  3,  3,  4,  3,  3,  3,  3,  1,  1,  2,  1,  3,  0,  1,  3,  3,  4,  3,  3,  2,  3,  3,  2,  2,  3,  2,
	3,  2,  4,  4,  3,  3,  3,  3,  1,  3,  5,  3,  3,  3,  3,  3,  3,  2,  3,  3,  6,  0,  2,  1,  2,  3,  4,  4,
	4,  1,  3,  3,  4,  4,  2,  4,  3,  2,  4,  3,  3,  4,  3,  3,  2,  4,  4,  4,  3,  3,  2,  1,  4,  3,  3,  3,
	4,  2,  1,  4,  5,  4,  2,  4,  2,  1,  2,  3,  4,  4,  2,  2,  1,  2,  1,  2,  2,  1,  1,  3,  3,  3,  5,  1,
	2,  5,  3,  2,  5,  3,  3,  2,  3,  1,  1,  1,  6,  2,  1,  4,  4,  1,  4,  6,  5,  2,  2,  1,  6,  6,  2,  4,
	2,  2,  1,  2,  5,  4,  2,  4,  3,  2,  4,  3,  3,  4,  4,  3,  3,  3,  3,  1,  1,  1,  0,  3,  3,  3,  3,  3,
	1,  0,  2,  2,  4,  5,  2,  1,  0,  3,  1,  3,  1,  3,  1,  0,  2,  5,  2,  2,  5,  1,  0,  3,  2,  2,  5,  2,
	2,  2,  1,  4,  4,  4,  2,  2,  4,  0,  2,  4,  2,  4,  5,  4,  2,  1,  5,  4,  1,  2,  1,  2,  2,  2,  4,  2,
	5,  2,  1,  5,  4,  2,  1,  2,  4,  2,  4,  6,  2,  4,  4,  2,  2,  2,  5,  4,  4,  4,  1,  4,  2,  3,  6,  5,
	1,  4,  4,  1,  4,  6,  5,  3,  5,  3,  5,  3,  6,  5,  2,  1,  2,  4,  4,  6,  5,  3,  3,  3,  3,  3,  6,  5,
	5,  5,  3,  3,  3,  3,  2,  2,  3,  3,  2,  4,  3,  2,  4,  2,  1,  4,  5,  4,  2,  2,  4,  0,  0,  5,  4,  2,
	1,  0,  2,  5,  2,  2,  5,  4,  0,  5,  5,  5,  3,  4,  3,  3,  1,  4,  3,  1,  2,  5,  1,  3,  3,  3,  5,  5,
	2,  2,  5,  4,  5,  4,  4,  2,  2,  3,  4,  5,  4,  2,  2,  2,  1,  2,  5,  4,  2,  3,  3,  3,  2,  5,  4,  3,
	3,  2,  2,  2,  4,  3,  3,  2,  3,  3,  2,  4,  3,  2,  4,  2,  2,  3,  3,  5,  2,  2,  2,  3,  4,  5,  4,  6,
};

// Init the ai tables
void ai_init(void)
{
	// Loop over 5 value ranges, and 0 to 4 pieces of each player
	// in a row

	for(char k=0; k<5; k++)
	{
		for(char i=0; i<5; i++)
		{
			for(char j=0; j<5; j++)
			{
				if (i && j)
					fscore[25 * k + PLAYER2_INC * i + j] = 0;
				else if (i == 4 || j == 4)
					fscore[25 * k + PLAYER2_INC * i + j] = score[i] - score[j];
				else
					fscore[25 * k + PLAYER2_INC * i + j] = (score[i] - score[j]) * (1 + k);
			}
		}
	}

	// Create the indices of all 69 rows

	char	p = 0;

	// Vertical rows
	for(char x=0; x<7; x++)
	{
		for(char y=0; y<3; y++)
		{
			frows[0][p] = x + 8 * y;
			frows[1][p] = x + 8 * y + 8;
			frows[2][p] = x + 8 * y + 16;
			frows[3][p] = x + 8 * y + 24;
			frows[4][p] = 25 * (y + 1);
			p++;
		}
	}

	for(char x=0; x<4; x++)
	{
		// Horizontal rows
		for(char y=0; y<6; y++)
		{
			frows[0][p] = x + 8 * y;
			frows[1][p] = x + 8 * y + 1;
			frows[2][p] = x + 8 * y + 2;
			frows[3][p] = x + 8 * y + 3;
			frows[4][p] = y > 0 ? 25 * (y - 1) : 0;
			p++;
		}

		// Diagonal down rows
		for(char y=0; y<3; y++)
		{
			frows[0][p] = x + 8 * y;
			frows[1][p] = x + 8 * y + 9;
			frows[2][p] = x + 8 * y + 18;
			frows[3][p] = x + 8 * y + 27;
			frows[4][p] = 25 * (y + 1);
			p++;
		}

		// Diagonal up rows
		for(char y=3; y<6; y++)
		{
			frows[0][p] = x + 8 * y;
			frows[1][p] = x + 8 * y - 7;
			frows[2][p] = x + 8 * y - 14;
			frows[3][p] = x + 8 * y - 21;
			frows[4][p] = 25 * (y - 2);
			p++;
		}
	}
}

// Evaluate the current board state for te player

int board_eval(bool player)
{
	int	score = 0;

	// Loop through all rows
	for(char r=0; r<69; r++)
	{
		// Build score index for row
		char	sum = 
			board.fcols[frows[0][r]] +
			board.fcols[frows[1][r]] +
			board.fcols[frows[2][r]] +
			board.fcols[frows[3][r]] +
			frows[4][r];

		// Help compiler to avoid two byte pointer arithmetic
		__assume(sum < 128);

		// Add score to board score
		score += fscore[sum];
	}

	// Check if there was a row with four winning pieces and
	// normalize to +/- 32000 then

	if (score < -8192)
		score = -32000;
	else if (score >= 8192)
		score = 32000;

	// Invert score for other player

	return player ? score : -score;
}

CharWin	cwt, cws;
CharWin	cw, cwm;

char	buff[20];

static const char optx[7] = {3, 2, 4, 1, 5, 0, 6};

// Find a good move for the player using alpha/beta pruning
// see: https://en.wikipedia.org/wiki/Alpha-beta_pruning

int board_check(char level, int alpha, int beta, bool player)
{
	int		best = alpha;
	bool	checked = false;

	// Current score to cutof early if clear win condition
	int 	val = board_eval(player) + level;

	// Best move
	char	zx = 0;

	// Check for end of search
	if (level < 5 && val < 5000 && val > -5000)
	{
		// Update current check display at level 3
		if (level == 3)
		{
			board_draw(&cwt);
			itoa(val, buff, 10);
			cwin_fill_rect(&cwt, 0, 6, 7, 1, ' ', 1);
			cwin_putat_string(&cwt, 0, 6, buff, 1);
		}

		// Check all seven columns
		for(char i=0; i<7; i++)
		{
			// Start check in the center, due to higher value of
			// pieces there

			char ix = optx[i];

			// Try to drop a piece

			if (board_drop(ix, player))
			{
				// We still have a move
				checked = true;

				// Check score of position
				int	score = - board_check(level + 1, -beta, -best, !player);

				// Better than what we have so far
				if (score > best)
				{
					// Remember
					best = score;						
					zx = ix;

					// Update best move if on level zero
					if (level == 0) 
					{
						board_draw(&cw);
						itoa(best, buff, 10);
						cwin_fill_rect(&cw, 0, 6, 7, 1, ' ', 1);
						cwin_putat_string(&cw, 0, 6, buff, 1);		
					}

					// Check for beta pruning
					if (best > beta)
					{
						board_undrop(ix);
						break;
					}
				}

				// Undo move
				board_undrop(ix);
			}
		}
	}

	// Return new position if first move level
	if (level == 0)
		return zx;
	else if (checked)		
		return best;
	else
		return val;
}

// Return a move from the opening library if available

char board_opening(char step, char * moves)
{
	switch (step)
	{
		case 1:
			return open1;
		case 2:
			if (moves[0] < 4)
				return open2[moves[0] * 7 + moves[1]];
			else
				return 6 - open2[(6 - moves[0]) * 7 + (6 - moves[1])];

		case 3:
			if (moves[0] < 4)
				return open3[moves[0] * 49 + moves[1] * 7 + moves[2]];
			else
				return 6 - open3[(6 - moves[0]) * 49 + (6 - moves[1]) * 7 + (6 - moves[2])];

		case 4:
			if (moves[0] < 4)
				return open4[moves[0] * 343 + moves[1] * 49 + moves[2] * 7 + moves[3]];
			else
				return 6 - open4[(6 - moves[0]) * 343 + (6 - moves[1]) * 49 + (6 - moves[2]) * 7 + (6 - moves[3])];

		default:
			return 0xff;
		}
}

// Current state of the game
struct Game
{
	GameState	state;		// State
	char		count;		// Auto continue counter
	char		posx;		// Column osition of new piece
	char		step;		// Number of player moves
	char		moves[21];	// Pieces placed by player

}	TheGame;

// Set new game state

void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	// Clear status line

	cwin_fill_rect(&cws, 0, 0, 40, 1, ' ', 1);

	switch (state)
	{
	case GS_READY:
		// Start play in one second
		TheGame.count = 60;
		TheGame.step = 0;

		// Clear the board
		board_init();
		board_init_main(&cwm);
		break;

	case GS_PLAYER_MOVE:
		// Place selection sprite in center
		TheGame.posx = 3;
		column_select_anim(&cwm, 3, 3, 7);		
		cwin_putat_string(&cws, 0, 0, P"PLAYER MOVE", 7);
		break;		

	case GS_COMPUTER_MOVE:
		cwin_putat_string(&cws, 0, 0, P"COMPUTER MOVE", 2);
		break;

	case GS_PLAYER_WIN:
		cwin_putat_string(&cws, 0, 0, P"PLAYER WINS", 7);

		// Continue in 4 seconds
		TheGame.count = 240;
		break;

	case GS_COMPUTER_WIN:
		cwin_putat_string(&cws, 0, 0, P"COMPUTER WINS", 2);

		// Continue in 4 seconds
		TheGame.count = 240;
		break;

	case GS_GAME_OVER:
		cwin_putat_string(&cws, 0, 0, P"NO MORE MOVES", 1);

		// Continue in 4 seconds
		TheGame.count = 240;
		break;
	}
}

// Main game loop

void game_loop()
{
	char	bx = 0xff, cx;

	switch (TheGame.state)
	{
	case GS_READY:
		if (!--TheGame.count)
			game_state(GS_PLAYER_MOVE);	
		break;

	case GS_PLAYER_MOVE:
		// Current selection
		cx = TheGame.posx;

		// Check for key pressed
		if (kbhit())
		{
			// Get key
			char	ch = getch();	

			// Numbers 1 to 7 drop immediate
			if (ch >= '1' && ch <= '7')
				bx = cx = ch - '1';
		}
		else
		{
			// Check Joystick
			joy_poll(0);

			// Move cursor
			if (joyx[0] < 0 && cx > 0)
				cx--;
			else if (joyx[0] > 0 && cx < 6)
				cx++;
			else if(joyb[0])
				bx = cx;
		}

		// Move selection piece
		column_select_anim(&cwm, TheGame.posx, cx, 7);
		TheGame.posx = cx;

		if (bx != 0xff)
		{
			// Drop piece into board
			if (board_drop(bx, true))
			{
				// Remember move
				TheGame.moves[TheGame.step++] = bx;

				// Play drop animation
				item_drop_anim(&cwm, bx, board.ffree[bx], 7);

				// Show new state of the board
				board_draw_main(&cwm);
				spr_show(0, false);

				// Check for win condition
				if (board_eval(true) == 32000)
					game_state(GS_PLAYER_WIN);
				else
					game_state(GS_COMPUTER_MOVE);
			}
		}
		break;

	case GS_COMPUTER_MOVE:
		// Check for opening move or calculate next move
		bx = board_opening(TheGame.step, TheGame.moves);
		if (bx == 0xff)
			bx = board_check(0, -32767, 32767, false);

		// Drop piece into board
		if (board_drop(bx, false))
		{
			// Play drop animation
			item_drop_anim(&cwm, bx, board.ffree[bx], 2);

			// Show new state of the board
			board_draw_main(&cwm);
			spr_show(0, false);

			// Check for loss or draw condition
			if (board_eval(false) == 32000)
				game_state(GS_COMPUTER_WIN);
			else if (TheGame.step == 21)
				game_state(GS_GAME_OVER);
			else
				game_state(GS_PLAYER_MOVE);
		}
		break;

	case GS_PLAYER_WIN:
	case GS_COMPUTER_WIN:
	case GS_GAME_OVER:
		if (!--TheGame.count)
			game_state(GS_READY);	
		break;
	}
}


int main(void)
{
	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_RAM);
	memcpy(Font, charset, 2048);
	memcpy(Sprites, spriteset, 256);
	mmap_set(MMAP_NO_BASIC);

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	// Change colors
	vic.color_border = VCOL_BLUE;
	vic.color_back = VCOL_BLACK;
	vic.color_back1 = VCOL_BLUE;
	vic.color_back2 = VCOL_WHITE;

	vic.spr_mcolor0 = VCOL_DARK_GREY;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Clear screen
	memset(Screen, 96, 1000);
	memset(Color, 15, 1000);

	// Prepare char wins
	cwin_init(&cws, Screen, 5, 0, 30, 1);
	cwin_init(&cwm, Screen, 2, 3, 28, 19);
	cwin_init(&cw, Screen, 30, 6, 8, 7);
	cwin_init(&cwt, Screen, 30, 15, 8, 7);

	// Init AI
	ai_init();

	game_state(GS_READY);

	for(;;)		
	{
		game_loop();
		vic_waitFrame();
	}	


	return 0;
}
