#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <c64/rasterirq.h>
#include <c64/cia.h>
#include <gfx/mcbitmap.h>
#include <string.h>
#include <stdlib.h>

// Make some room
#pragma region(main, 0x0a00, 0xc800, , , {code, data, bss, heap, stack} )

// Sprite assets
const char MissileSprites[] = {
#embed "../resources/missilesprites.bin"	
};

// Charset assets
const char MissileChars[] = {
#embed "../resources/missilechars.bin"	
};

// Graphics areas in bank 3
#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)
#define Hires	((char *)0xe000)
#define Sprites	((char *)0xd000)
#define Charset ((char *)0xd800)

// Joystick and crosshair control
int		CrossX = 160, CrossY = 100;
bool	CrossP = false;
char	CrossDelay = 0;

// Display bitmap
Bitmap			sbm;
const ClipRect	scr = { 0, 0, 320, 200 };

// Structure for a explosion
struct Explosion
{
	int				x, y;	// Center of circle
	char			r;		// Radius of circle
	Explosion	*	next;	// Next explosion in list
};

// Structure for a missile (defensive and ICBM)
struct Missile
{
	int				sx, sy;	// start position
	int				tx, ty;	// target position
	int				x, y;	// current position
	int				dx, dy;	// distance in x and y
	int				d;		// error term for Bresenham
	sbyte			stepx;	// direction in x (+1 or -1)
	sbyte			cnt;	// speed counter
	Missile		*	next;	// next missile in list
};

// Storage space for explosion
Explosion		explosions[16];
// First free and first used explosion
Explosion	*	efree, * eused;	

// Storage space for defending missiles
Missile			missiles[8];
// First free and first used missile
Missile		*	mfree, * mused;
// Number of missiles available
char			nmissiles;

// Storage space for ICMBs
Missile			icbms[16];
// First free and first used ICBM
Missile		*	ifree, * iused;
// Speed and number of ICBMs still incomming
char			icbmspeed, icbmcount;

// Cities not yet destroyed
bool 			cities[6];
char			ncities;

// Init status bar at top
void status_init(void)
{
	memset(Color1, ' ', 40);
	memset(Color2, 1, 40);

	for(char i=0; i<8; i++)	
	{
		Color1[i +  2] = '0';
		Color1[i + 12] = '0';
		Color2[i + 12] = 7;
	}
}

// Expand an 8x8 charactor to 16x16 on screen
void char_put(char cx, char cy, char c, char color)
{
	// Get pointer to glyph data
	const char	*	sp = MissileChars + 8 * c;

	// Loop over all pixel
	for(char y=0; y<8; y++)
	{
		char cl = sp[y];
		for(char x=0; x<8; x++)
		{
			// Draw two pixel if bit is set
			if (cl & 128)
			{
				bmmc_put(&sbm, cx + 2 * x, cy + 2 * y + 0, color);
				bmmc_put(&sbm, cx + 2 * x, cy + 2 * y + 1, color);
			}

			// Next bit
			cl <<= 1;
		}
	}
}

// Write a zero terminated string on screen
void char_write(char cx, char cy, const char * s, char color)
{
	// Loop over all characters
	while (*s)
	{
		char_put(cx, cy, *s, color);
		s++;
		cx += 16;
	}
}

// Increment the score from a given digit on
void score_inc(char digit, unsigned val)
{
	// Lowest digit to increment
	char	at = 9 - digit;

	// Loop while there is still score to account for
	while (val)
	{
		// Increment one character
		char	ch = Color1[at] + val % 10;

		// Remove low digit from number
		val /= 10;

		// Check overflow
		if (ch > '9')
		{
			ch -= 10;
			val++;
		}

		Color1[at] = ch;

		// Next higher character
		at --;
	}
}

// Reset score and update high score
void score_reset(void)
{
	// Find first digit, where score and highscore differ
	char i = 0;
	while (i < 8 && Color1[i + 2] == Color1[i + 12])
		i++;

	// Check if new score is higher
	if (i < 8 && Color1[i + 2] > Color1[i + 12])
	{
		// If so, copy to highscore
		while (i < 8)
		{
			Color1[i + 12] = Color1[i + 2];
			i++;
		}
	}

	// Clear score
	for(char i=0; i<8; i++)	
		Color1[i + 2] = '0';	
}

// Update number of missiles
void status_missiles(char num)
{
	char	n = 0;

	// Draw full pairs
	while (2 * n + 1 < num)
	{
		Color1[25 + n] = 92;
		n++;
	}
	// Draw remaining single one
	if (num & 1)
	{
		Color1[25 + n] = 93;
		n++;
	}
	// Empty remainder
	while (n < 15)
	{
		Color1[25 + n] = 94;
		n++;
	}
}

// Initialize explosion list
void explosion_init(void)
{
	// No explosion active
	eused = nullptr;

	// First free explosion element
	efree = explosions;
	// Build list
	for(char i=0; i<15; i++)
		explosions[i].next = explosions + i + 1;
	// Terminate last element
	explosions[15].next = nullptr;
}

// Start a new explosion
void explosion_start(int x, int y)
{
	// Free slot in list of explosions?
	if (efree)
	{
		// Move entry from free to used list
		Explosion	*	e = efree;
		efree = e->next;
		e->next = eused;
		eused = e;

		// Initialize position and size
		e->r = 0;
		e->x = x;
		e->y = y;
	}
}

// Animate all explosions
void explosion_animate(void)
{
	// Loop over active explosions with "e", use "ep" to point
	// to previous explosion, so we can remove the current explosion
	// from the list
	Explosion	*	e = eused, * ep = nullptr;
	while (e)
	{
		// Remember next entry in list
		Explosion	*	en = e->next;

		// Increment phase (radius)
		e->r++;

		// Advance every fourth frame
		if (!(e->r & 3))
		{
			// Draw or erase outer perimeter depending on growing or
			// shrinking explosion phase
			if (e->r <= 64)
				bmmc_circle(&sbm, &scr, e->x, e->y, e->r >> 2, 1);
			else
				bmmc_circle(&sbm, &scr, e->x, e->y, 33 - (e->r >> 2), 0);
		}

		// End of explosion live
		if (e->r == 128)
		{
			// Remove explosion from used list
			if (ep)
				ep->next = e->next;
			else
				eused = e->next;

			// Prepend it to free list
			e->next = efree;
			efree = e;
		}
		else
			ep = e;

		// Next explosion in list
		e = en;
	}
}

// Initialize defending missile list
void missile_init(void)
{
	mused = nullptr;
	mfree = missiles;
	for(char i=0; i<7; i++)
		missiles[i].next = missiles + i + 1;
	missiles[7].next = nullptr;
}

// Add a new defending missile
void missile_start(int sx, int sy, int tx, int ty)
{
	// Check if entry in free list and missile in silo remaining
	if (mfree && nmissiles > 0)
	{
		// Detach from free list
		Missile	*	m = mfree;
		mfree = m->next;

		// Attach to active list
		m->next = mused;
		mused = m;

		// Initialize start and target coordinates
		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;

		// Initialize line drawing parameters
		m->dy = m->sy - m->ty;
		m->dx = m->tx - m->sx;
		m->stepx = 1;
		if (m->dx < 0)
		{
			m->dx = -m->dx;
			m->stepx = -1;
		}

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;

		// Remove missile from silo
		nmissiles--;
		status_missiles(nmissiles);
	}
}

// Animate all active missiles
void missile_animate(void)
{
	Missile	*	m = mused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		// Advance missile position using one step of Bresenham
		if (m->d >= 0)
		{
			m->y--;
			m->d -= m->dx;
		}
		if (m->d < 0)
		{
			m->x += m->stepx;
			m->d += m->dy;
		}

		// Check if target reached
		if (m->y == m->ty)
		{
			// If so, clear line and start explosion
			bmmcu_line(&sbm, m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
			explosion_start(m->x * 2, m->y);

			// Remove from active list
			if (mp)
				mp->next = m->next;
			else
				mused = m->next;
			m->next = mfree;
			mfree = m;
		}
		else
		{
			// Draw new pixel in missile trace
			bmmc_put(&sbm, m->x * 2, m->y, 3);
			mp = m;
		}

		m = mn;
	}
}

// Initialize incomming ICBM list
void icbm_init(void)
{
	iused = nullptr;
	ifree = icbms;
	for(char i=0; i<15; i++)
		icbms[i].next = icbms + i + 1;
	icbms[15].next = nullptr;
}

// Add a new ICBM to the attacking set
void icbm_start(int sx, int sy, int tx, int ty)
{
	if (icbmcount && ifree)
	{
		Missile	*	m = ifree;
		ifree = m->next;
		m->next = iused;
		iused = m;

		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;
		m->cnt = 0;

		m->dy = m->ty - m->sy;
		m->dx = m->tx - m->sx;
		m->stepx = 1;		
		if (m->dx < 0)
		{
			m->dx = -m->dx;
			m->stepx = -1;
		}

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;

		icbmcount--;
	}
}

void icbm_animate(void)
{
	Missile	*	m = iused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		// Check speed of ICBMs
		m->cnt += icbmspeed;
		while (m->cnt > 0)
		{
			m->cnt -= 32;

			// Draw pixel in trace
			bmmc_put(&sbm, m->x * 2, m->y, 2);			

			// Advance using Bresenham
			if (m->d >= 0)
			{
				m->y++;
				m->d -= m->dx;
			}
			if (m->d < 0)
			{
				m->x += m->stepx;
				m->d += m->dy;
			}

			// Check if colliding with cloud or target reached
			if (bmmc_get(&sbm, m->x * 2, m->y) == 1 || m->y == m->ty)
			{
				// If so, clear trace and start explosion
				bmmcu_line(&sbm, m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
				explosion_start(m->x * 2, m->y);

				// Check if we hit the ground
				if (m->y == m->ty)
				{
					// If so, find matching city
					int	x = m->x * 2;
					char	ix;
					if (x > 160)
						ix = ((x - 202) >> 5) + 3;
					else
						ix = (x - 58) >> 5;

					// Destroy, destroy, annihilate, kill, kill
					if (cities[ix])
					{
						ncities--;
						cities[ix] = false;
						spr_show(ix + 1, false);
					}
				}

				// Add score for destroyed ICBM
				score_inc(0, 25);

				// Remove from list
				if (mp)
					mp->next = m->next;
				else
					iused = m->next;
				m->next = ifree;
				ifree = m;
				m = nullptr;

				// End loop early
				break;
			}
		}

		// If ICBM still in flight
		if (m)
		{
			// Remember for list management
			mp = m;
			// Draw white head
			bmmc_put(&sbm, m->x * 2, m->y, 1);
		}

		m = mn;
	}
}

// Initialize game screen
void screen_init(void)
{
	// Clean up
	bmmcu_rect_fill(&sbm, 0, 8, 320, 184, 0);
	
	// Draw bottom
	bmmcu_rect_fill(&sbm, 0, 192, 320, 8, 3);
	bmmc_quad_fill(&sbm, &scr, 0, 192, 16, 184, 32, 184, 48, 192, MixedColors[3][3]);
	bmmc_quad_fill(&sbm, &scr, 136, 192, 152, 184, 176, 184, 192, 192, MixedColors[3][3]);
	bmmc_quad_fill(&sbm, &scr, 272, 192, 288, 184, 304, 184, 320, 192, MixedColors[3][3]);

	// Show cities
	for(char i=0; i<3; i++)
	{
		if (cities[i + 0])
			spr_set(i + 1, true, 70 + 32 * i, 222, 65, 15, false, false, false);		
		if (cities[i + 3])
			spr_set(i + 4, true, 214 + 32 * i, 222, 65, 15, false, false, false);		
	}
}

// Clear inner area of screen
void screen_clear(void)
{
	bmmcu_rect_fill(&sbm, 0, 8, 320, 176, 0);	
}

// Interrupt routine for joystick control, called by raster IRQ at bottom
// of screen

__interrupt void joy_interrupt()
{
	// Poll joystick
	joy_poll(0);

	// Move crosshair coordinates
	CrossX += 2 * joyx[0]; CrossY += 2 * joyy[0];

	// Stop at edges of screen
	if (CrossX < 8)
		CrossX = 8;
	else if (CrossX > 312)
		CrossX = 312;
	if (CrossY < 20)
		CrossY = 20;
	else if (CrossY > 172)
		CrossY = 172;

	// Move crosshair sprite
	spr_move(0, CrossX + 14, CrossY + 40);	

	// Check button
	if (joyb[0])
	{
		// Avoid quickfire and bouncing
		if (CrossDelay == 0)
		{
			// Request fire from non interrupt code
			CrossP = true;
			CrossDelay = 4;
		}
	}
	else if (CrossDelay > 0)
		CrossDelay--;
}

enum GameState
{
	GS_READY,		// Getting ready
	GS_LEVEL,		// Show level message
	GS_PLAYING,		// Playing the game
	GS_BONUS,		// Landed on pad
	GS_ARMAGEDDON, 	// Show end game animation
	GS_END			// Wait for restart
};

// State of the game
struct Game
{
	GameState	state;	
	byte		count, level;

}	TheGame;	// Only one game, so global variable

// Character buffer for some texts
char	cbuffer[10];

void game_state(GameState state)
{

	TheGame.state = state;

	switch(state)
	{
	case GS_READY:	
		// Start of new game
		score_reset();
		screen_init();
		char_write(40, 100, s"READY PLAYER 1", 3);

		// New cities
		for(char i=0; i<6; i++)
			cities[i] = true;
		ncities = 6;
		TheGame.count = 60;

		// Starting at level 1
		TheGame.level = 0;
		break;

	case GS_LEVEL:
		// Advance to next level
		TheGame.level++;
		utoa(TheGame.level ,cbuffer, 10);
		screen_clear();
		char_write(96, 100, s"LEVEL", 3);
		char_write(96 + 16 * 6, 100, cbuffer, 3);
		TheGame.count = 30;
		break;

	case GS_PLAYING:
		// Avoid old fire request
		CrossP = false;

		// Setup display
		screen_init();
		missile_init();
		explosion_init();
		icbm_init();

		// A new set of 30 missiles
		nmissiles = 30;
		status_missiles(nmissiles);

		// Game parameters based on level
		if (TheGame.level < 112)
			icbmspeed = 8 + TheGame.level;
		else
			icbmspeed = 120;

		if (TheGame.level < 50)
			icbmcount = 5 + TheGame.level / 2;
		else
			icbmcount = 30;

		TheGame.count = 15;
		break;

	case GS_BONUS:
	{
		// Show bonus

		unsigned bonus = ncities * 50 + nmissiles * 5;
		utoa(bonus, cbuffer, 10);

		char_write(96, 100, s"BONUS", 3);
		char_write(96 + 16 * 6, 100, cbuffer, 3);

		score_inc(0, bonus);
		TheGame.count = 30;
	}	break;

	case GS_ARMAGEDDON:
		TheGame.count = 0;
		break;

	case GS_END:
		char_write(104, 92, s"THE END", 0);
		TheGame.count = 120;
		break;
	}
}

// Main game play code
void game_play(void)
{
	// Check if fire request
	if (CrossP)
	{
		// Find lauch site
		int	sx = 160;
		if (CrossX < 120)
			sx = 24;
		else if (CrossX > 200)
			sx = 296

		// Fire missile
		missile_start(sx, 184, CrossX, CrossY);

		// Reset request
		CrossP = false;
	}

	// Wait for next ICMB to enter the game
	if (!--TheGame.count)
	{	
		// Pick target city
		char ci;
		do {
			ci = rand() & 7;
		} while (ci >= 6 || !cities[ci]);

		int	cx;
		if (ci < 3)
			cx = 58 + 32 * ci;
		else
			cx = 202 + 32 * (ci - 3);

		// Launch ICBM
		icbm_start((rand() & 0xff) + 32, 0, cx, 184);

		// Next lauch time
		TheGame.count = 8 + (rand() & 63);
	}

	// Advance defending missiles by four pixels
	for(char i=0; i<4; i++)		
		missile_animate();

	// Advance ICBMs
	icbm_animate();

	// Show explosions
	explosion_animate();
}

// Main game loop, entered every VSYNC, slower if too busy with explosions
void game_loop(void)
{
	switch(TheGame.state)
	{
	case GS_READY:
		if (!--TheGame.count)
			game_state(GS_LEVEL);
		break;
	case GS_LEVEL:
		if (!--TheGame.count)
			game_state(GS_PLAYING);
		break;

	case GS_PLAYING:
		game_play();

		// Check for level and game end coditions
		if (!icbmcount && !iused && !eused)
			game_state(GS_BONUS);
		else if (ncities == 0)
			game_state(GS_ARMAGEDDON);
		break;

	case GS_BONUS:
		if (!--TheGame.count)
			game_state(GS_LEVEL);
		break;

	case GS_ARMAGEDDON:
		// Draw end game animation
		TheGame.count++;		
		bmmc_circle(&sbm, &scr, 160, 100, TheGame.count, 1);
		explosion_animate();
		if (TheGame.count == 90)
			game_state(GS_END);
		break;

	case GS_END:
		if (!--TheGame.count)
			game_state(GS_READY);
		break;
	}
}

// Interrupts for status line and joystick routine
RIRQCode	bottom, top;

int main(void)
{
	// Activate trampoline
	mmap_trampoline();

	// Disable CIA interrupts, we do not want interference
	// with our joystick interrupt
	cia_init();

	// Copy assets
	mmap_set(MMAP_RAM);
	memcpy(Sprites, MissileSprites, 1024);
	memcpy(Charset, MissileChars, 2048);
	mmap_set(MMAP_NO_ROM);

	// Clean out screen space
	memset(Color1, 0x18, 1000);
	memset(Color2, 0x06, 1000);
	memset(Hires, 0, 8000);
	memset(Color2 + 40 * 23, 0x07, 80);

	// initialize raster IRQ
	rirq_init(true);

	// Switch to hires mode
	vic_setmode(VICM_HIRES_MC, Color1, Hires);
	spr_init(Color1);

	// Black background and border
	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	// Init bitmap
	bm_init(&sbm, Hires, 40, 25);

	// Init status line
	status_init();

	// Init cross hair sprite
	spr_set(0, true, CrossX + 14, CrossY + 40, 64, 1, false, false, false);

	// Build to multicolor highres at top of screen
	rirq_build(&top, 3);
	rirq_delay(&top, 10);
	rirq_write(&top, 1, &vic.ctrl1, VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	rirq_write(&top, 2, &vic.memptr, 0x28);
	rirq_set(0, 57, &top);

	// Switch to text mode for status line and poll joystick at bottom
	rirq_build(&bottom, 3);
	rirq_write(&bottom, 0, &vic.memptr, 0x27);
	rirq_write(&bottom, 1, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	rirq_call(&bottom, 2, joy_interrupt);
	rirq_set(1, 250, &bottom);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	// start game state machine
	game_state(GS_READY);
	for(;;)
	{
		game_loop();
		rirq_wait();
	}

	return 0;
}
