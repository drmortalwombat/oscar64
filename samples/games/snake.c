#include <c64/joystick.h>
#include <c64/vic.h>
#include <stdlib.h>
#include <string.h>


// Position/Direction on screen
struct Point
{
	sbyte	x, y;
};

struct Snake
{
	Point	head;		// Position of head
	Point	dir;		// Direction of head
	Point	tail[256];	// Position of tail
	byte	length;		// Length of tail
	byte	pos;		// Tail start
};

enum GameState
{
	GS_READY,		// Getting ready
	GS_PLAYING,		// Playing the game
	GS_COLLIDE		// Collided with something
};

// State of the game
struct Game
{
	GameState	state;	
	byte		count;
	Snake		snake;	// use an array for multiplayer

}	TheGame;	// Only one game, so global variable


// Screen and color ram address

#define Screen ((byte *)0x0400)
#define Color ((byte *)0xd800)

// Put one  char on screen
inline void screen_put(byte x, byte y, char ch, char color)
{
	Screen[40 * y + x] = ch;
	Color[40 * y + x] = color;
}

// Get one char from screen
inline char screen_get(byte x, byte y)
{
	return Screen[40 * y + x];
}

// Put a fruit/heart at random position
void screen_fruit(void)
{
	byte x, y;
	do
	{		
		// Draw a random position
		x = 1 + rand() % 38;
		y = 1 + rand() % 23;	

		// Ensure it is an empty place	
	} while (screen_get(x, y) != ' ');

	// Put the heart on screen
	screen_put(x, y, 83, VCOL_YELLOW);

}

// Clear screen and draw borders
void screen_init(void)
{
	// Fill screen with spaces
	memset(Screen, ' ', 1000);

	// Bottom and top row
	for(byte x=0; x<40; x++)
	{
		screen_put(x,  0, 0xa0, VCOL_LT_GREY);
		screen_put(x, 24, 0xa0, VCOL_LT_GREY);
	}

	// Left and right column
	for(byte y=0; y<25; y++)
	{
		screen_put( 0,  y, 0xa0, VCOL_LT_GREY);
		screen_put( 39, y, 0xa0, VCOL_LT_GREY);			
	}

}

// Initialize a snake
void snake_init(Snake * s)
{
	// Length of tail is one
	s->length = 1;
	s->pos = 0;

	// Snake in the center of screen
	s->head.x = 20;
	s->head.y = 12;

	// Starting to the right
	s->dir.x = 1;
	s->dir.y = 0;

	// Show head
	screen_put(s->head.x, s->head.y, 81, VCOL_WHITE);
}

bool snake_advance(Snake * s)
{
	// Promote head to start of tail
	s->tail[s->pos] = s->head;
	s->pos++;

	screen_put(s->head.x, s->head.y, 81, VCOL_LT_BLUE);

	// Advance head
	s->head.x += s->dir.x;
	s->head.y += s->dir.y;

	// Get character at new head position
	char ch = screen_get(s->head.x, s->head.y);

	// Draw head
	screen_put(s->head.x, s->head.y, 81, VCOL_WHITE);

	// Clear tail
	char	tpos = s->pos - s->length;
	screen_put(s->tail[tpos].x, s->tail[tpos].y, ' ', VCOL_BLACK);

	// Did snake collect the fruit
	if (ch == 83)
	{
		// Extend tail
		s->length++;
		screen_fruit();
	}
	else if (ch != ' ')
	{
		// Snake collided with something
		return true;
	}

	return false;
}

// flash the snake after collision
void snake_flash(Snake * s, char c)
{
	// Loop over all tail elements
	for(char i=0; i<s->length; i++)
	{
		// Set color
		char tpos = s->pos - i - 1;
		screen_put(s->tail[tpos].x, s->tail[tpos].y, 81, c);
	}
}

// Change snake direction based on user input
void snake_control(Snake * s, sbyte jx, sbyte jy)
{
	// First change from horizontal to vertical, otherwise
	// check vertical to horizontal
	if (s->dir.x && jy)
	{
		s->dir.x = 0;
		s->dir.y = jy;
	}
	else if (s->dir.y && jx)
	{
		s->dir.y = 0;
		s->dir.x = jx;
	}
}

void game_state(GameState state)
{
	// Set new state
	TheGame.state = state;

	switch(state)
	{
	case GS_READY:
		// Clear the screen
		screen_init();
		TheGame.count = 32;
		break;

	case GS_PLAYING:
		// Init the snake
		snake_init(&TheGame.snake);

		// Initial fruit
		screen_fruit();

		TheGame.count = 16;
		break;

	case GS_COLLIDE:
		TheGame.count = 16;
		break;
	}
}

// Colors for collision "animation"
char FlashColors[] = {
	VCOL_YELLOW,
	VCOL_WHITE,
	VCOL_LT_GREY,
	VCOL_YELLOW,
	VCOL_ORANGE,
	VCOL_RED,
	VCOL_MED_GREY,
	VCOL_DARK_GREY
};

// Main game loop, invoked every vsync
void game_loop(void)
{
	switch (TheGame.state)
	{
	case GS_READY:
		// Countdown ready to start
		if (!--TheGame.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:
		// Check player input on every frame
		joy_poll(0);
		snake_control(&TheGame.snake, joyx[0], joyy[0]);

		if (!--TheGame.count)
		{
			// Move snake every four frames, advance to collision
			// state if collided
			if (snake_advance(&TheGame.snake))
				game_state(GS_COLLIDE);
			else
				TheGame.count = 4;
		}
		break;
	case GS_COLLIDE:
		// Flash the collided snake
		snake_flash(&TheGame.snake, FlashColors[(16 - TheGame.count) / 2]);
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}

}



int main(void)
{
	// Screen color to black
	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	// Start the game in ready state	
	game_state(GS_READY);

	// Forever
	for(;;)
	{
		// One game loop iteration
		game_loop();

		// Wait one vsync
		vic_waitFrame();
	}

	// Never reached
	return 0;
}
