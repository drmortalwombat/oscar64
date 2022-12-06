#include <c64/vic.h>
#include <c64/memmap.h>
#include <string.h>
#include <stdlib.h>

static char * const Screen	=	(char *)0xc800;
static char * const Color	=	(char *)0xd800; 
static char * const Hires	=	(char *)0xe000;

// Single particle, with position, veloicty and color pattern, using a next
// pointer for single linked list
struct Particle
{
	int				px, py, vx, vy;
	char			pat;
	Particle	*	next;
};

// Storage for up to 256 particles
Particle	particles[256];

// Heads of used and free list
Particle	*	pfirst, * pfree;

// Lookup table for hires row buffer
static char * Hirows[25];

// Pixel masks
static const char setmask[8] = {0xc0, 0xc0, 0x30, 0x30, 0x0c, 0x0c, 0x03, 0x03};
static const char clrmask[8] = {0x3f, 0x3f, 0xcf, 0xcf, 0xf3, 0xf3, 0xfc, 0xfc};

// Set a pixel at the given coordiate
void pix_set(unsigned px, unsigned py, char pat)
{
	__assume(px < 320);
	__assume(py < 200);

	// Calculate base position in hires
	char * dp = Hirows[py >> 3] + (px & ~7);

	// Set two pixels for a square pixel look
	char ly = py & 6;
	dp[ly + 1] = dp[ly + 0] |= setmask[px & 7] & pat;
}

// Clear a pixel at the given coordiate
void pix_clr(unsigned px, unsigned py)
{
	__assume(px < 320);
	__assume(py < 200);	

	// Calculate base position in hires
	char * dp = Hirows[py >> 3] + (px & ~7);

	// Clear two pixels for a square pixel look
	char ly = py & 6;
	dp[ly + 1] = dp[ly + 0] &= clrmask[px & 7];
}

// Init free list of particles
void particle_init(void)
{
	// Init address table for hires
	for(int i=0; i<25; i++)
		Hirows[i] = Hires + 320 * i;

	// Init list heads
	pfirst = nullptr;
	pfree = particles;

	// Link all particles in free list
	for(int i=0; i<255; i++)
		particles[i].next = particles + i + 1;
}

// Add a particle to the list
void particle_add(int px, int py, int vx, int vy, char pat)
{
	// Check if we have a particle left
	if (pfree)
	{
		// Remove from free list
		Particle	*	p = pfree;
		pfree = pfree->next;

		// Add to used list
		p->next = pfirst;
		pfirst = p;

		// Init particle data
		p->px = px;
		p->py = py;
		p->vx = vx;
		p->vy = vy;
		p->pat = pat;
	}
}

// Move particles in used list
void particle_move(void)
{
	// Start with first particle, remember previous
	// particle for list removal
	Particle	*	p = pfirst, * pp = nullptr;

	// Loop over all particles in used list
	while (p)
	{
		// Clear previous particle image, using 10.6 fixed point
		pix_clr(p->px >> 6, p->py >> 6);

		// Advance position by velocity
		p->px += p->vx;
		p->py += p->vy;

		// Apply gravity
		p->vy += 8;

		// Check if particle is still on screen
		if (p->px < 0 || p->px >= 320 * 64 || p->py < 0 || p->py >= 200 * 64)
		{
			// Particle is offscreen, so we remove it from the used list

			// Remember next particle in used list
			Particle	*	pn = p->next;

			// Remove from used list
			if (pp)
				pp->next = pn;
			else
				pfirst = pn;

			// Attach to free list
			p->next = pfree;
			pfree = p;

			// Advance to next particle
			p = pn;
		}
		else
		{
			// Set image at new position
			pix_set(p->px >> 6, p->py >> 6, p->pat);

			// Advance to next particle
			pp = p;
			p = p->next;
		}
	}
}

// Normalized random function
int rnorm(void)
{
	int l0 = (rand() & 0xfff) - 0x800;
	int l1 = (rand() & 0xfff) - 0x800;
	int l2 = (rand() & 0xfff) - 0x800;
	int l3 = (rand() & 0xfff) - 0x800;

	return l0 + l1 + l2 + l3;
}

int main(void)
{
	// Turn off BASIC ROM
	mmap_set(MMAP_NO_BASIC);

	// Install IRQ trampoline
	mmap_trampoline();

	// Turn off kernal ROM
	mmap_set(MMAP_NO_ROM);

	// Switch to hires multicolor mode
	vic_setmode(VICM_HIRES_MC, Screen, Hires);

	// Clear screen
	memset(Screen, 0x78, 1000);
	memset(Color, 0x0e, 1000);
	memset(Hires, 0x00, 8000);

	// Black background
	vic.color_border = 0x00;
	vic.color_back = 0x00;

	// Init particle system
	particle_init();

	char k = 0;
	for(int i=0; i<10000; i++)
	{
		// Advance particles
		particle_move();

		if (k < 25)
		{
			// Add a particle from the left for the first third
			particle_add(4 * 64, 196 * 64, 256 + (rnorm() >> 6), -(384 + (rnorm() >> 6)), 0x55);
		}
		else if (k < 50)
		{
			// Add a particle from the right for the second third
			particle_add(316 * 64, 196 * 64, - (256 + (rnorm() >> 6)), -(384 + (rnorm() >> 6)), 0xaa);
		}
		else if (k < 75)
		{
			// Add a particle from the middle for the final third
			particle_add(160 * 64, 196 * 64, rnorm() >> 6, -(384 + (rnorm() >> 6)), 0xff);
		}
		
		// Advance thirds counter
		k++;
		if (k == 75)
			k = 0;
	}

	return 0;

}
