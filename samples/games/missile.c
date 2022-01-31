#include <c64/vic.h>
#include <c64/memmap.h>
#include <c64/sprites.h>
#include <c64/joystick.h>
#include <c64/rasterirq.h>
#include <gfx/mcbitmap.h>
#include <string.h>
#include <stdlib.h>

#pragma region(main, 0x0a00, 0xc800, , , {code, data, bss, heap, stack} )

const char MissileSprites[] = {
#embed "../resources/missilesprites.bin"	
};

const char MissileChars[] = {
#embed "../resources/missilechars.bin"	
};

#define Color1	((char *)0xc800)
#define Color2	((char *)0xd800)
#define Hires	((char *)0xe000)
#define Sprites	((char *)0xd000)
#define Charset ((char *)0xd800)

int		CrossX = 160, CrossY = 100;
bool	CrossP = false;
char	CrossDelay = 0;

Bitmap			sbm;
const ClipRect	scr = { 0, 0, 320, 200 };

struct Explosion
{
	int				x, y;
	char			r;
	Explosion	*	next;
};

struct Missile
{
	int				sx, sy, tx, ty, x, y, dx, dy;
	int				d;
	char			cnt;
	Missile		*	next;
};

Explosion		explosions[16];
Explosion	*	efree, * eused;

Missile			missiles[8];
Missile		*	mfree, * mused;

Missile			icbms[16];
Missile		*	ifree, * iused;

bool cities[6];


void explosion_init(void)
{
	eused = nullptr;
	efree = explosions;
	for(char i=0; i<15; i++)
		explosions[i].next = explosions + i + 1;
	explosions[15].next = nullptr;
}

void explosion_start(int x, int y)
{
	if (efree)
	{
		Explosion	*	e = efree;
		efree = e->next;
		e->next = eused;
		eused = e;

		e->r = 0;
		e->x = x;
		e->y = y;
	}
}

void explosion_animate(void)
{
	Explosion	*	e = eused, * ep = nullptr;
	while (e)
	{
		Explosion	*	en = e->next;
		e->r++;
		if (!(e->r & 3))
		{
			if (e->r <= 64)
				bmmc_circle(&sbm, &scr, e->x, e->y, e->r >> 2, 1);
			else
				bmmc_circle(&sbm, &scr, e->x, e->y, 33 - (e->r >> 2), 0);
		}
		if (e->r == 128)
		{
			if (ep)
				ep->next = e->next;
			else
				eused = e->next;
			e->next = efree;
			efree = e;
		}
		else
			ep = e;

		e = en;
	}
}

void missile_init(void)
{
	mused = nullptr;
	mfree = missiles;
	for(char i=0; i<7; i++)
		missiles[i].next = missiles + i + 1;
	missiles[7].next = nullptr;
}

void missile_start(int sx, int sy, int tx, int ty)
{
	if (mfree)
	{
		Missile	*	m = mfree;
		mfree = m->next;
		m->next = mused;
		mused = m;

		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;

		m->dy = m->sy - m->ty;
		m->dx = m->tx - m->sx;
		if (m->dx < 0)
			m->dx = -m->dx;

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;
	}
}

void missile_animate(void)
{
	Missile	*	m = mused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		if (m->d >= 0)
		{
			m->y--;
			m->d -= m->dx;
		}
		if (m->d < 0)
		{
			if (m->tx < m->sx)
				m->x--;
			else
				m->x++;
			m->d += m->dy;
		}

		if (bmmc_get(&sbm, m->x, m->y) == 1 || m->y == m->ty)
		{
			bmmcu_line(&sbm, m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
			explosion_start(m->x * 2, m->y);

			if (mp)
				mp->next = m->next;
			else
				mused = m->next;
			m->next = mfree;
			mfree = m;
		}
		else
		{
			bmmc_put(&sbm, m->x * 2, m->y, 3);
			mp = m;
		}

		m = mn;
	}
}

void icbm_init(void)
{
	iused = nullptr;
	ifree = icbms;
	for(char i=0; i<15; i++)
		icbms[i].next = icbms + i + 1;
	icbms[15].next = nullptr;
}

void icbm_start(int sx, int sy, int tx, int ty)
{
	if (ifree)
	{
		Missile	*	m = ifree;
		ifree = m->next;
		m->next = iused;
		iused = m;

		m->sx = sx >> 1; m->x = sx >> 1;
		m->sy = sy; m->y = sy;
		m->tx = tx >> 1;
		m->ty = ty;
		m->cnt = 4;

		m->dy = m->ty - m->sy;
		m->dx = m->tx - m->sx;
		if (m->dx < 0)
			m->dx = -m->dx;

		m->d = m->dy - m->dx;
		m->dx *= 2;
		m->dy *= 2;
	}
}

void icbm_animate(void)
{
	Missile	*	m = iused, * mp = nullptr;
	while (m)
	{
		Missile	*	mn = m->next;

		m->cnt--;

		if (!m->cnt)
		{
			bmmc_put(&sbm, m->x * 2, m->y, 2);

			m->cnt = 4;

			if (m->d >= 0)
			{
				m->y++;
				m->d -= m->dx;
			}
			if (m->d < 0)
			{
				if (m->tx < m->sx)
					m->x--;
				else
					m->x++;
				m->d += m->dy;
			}

			if (bmmc_get(&sbm, m->x * 2, m->y) == 1 || m->y == m->ty)
			{
				bmmcu_line(&sbm, m->sx * 2, m->sy, m->tx * 2, m->ty, 0);
				explosion_start(m->x * 2, m->y);

				if (m->y == m->ty)
				{
					int	x = m->x * 2;
					char	ix;
					if (x > 160)
						ix = (x - 202) / 32 + 3;
					else
						ix = (x - 58) / 32;
					cities[ix] = false;
					spr_show(ix + 1, false);
				}


				if (mp)
					mp->next = m->next;
				else
					iused = m->next;
				m->next = ifree;
				ifree = m;
			}
			else
			{
				bmmc_put(&sbm, m->x * 2, m->y, 1);
				mp = m;
			}
		}
		else
			mp = m;

		m = mn;
	}
}

void mountains_init(void)
{
	bmmcu_rect_fill(&sbm, 0, 192, 320, 8, 3);
	bmmc_quad_fill(&sbm, &scr, 0, 192, 16, 184, 32, 184, 48, 192, MixedColors[3][3]);
	bmmc_quad_fill(&sbm, &scr, 136, 192, 152, 184, 176, 184, 192, 192, MixedColors[3][3]);
	bmmc_quad_fill(&sbm, &scr, 272, 192, 288, 184, 304, 184, 320, 192, MixedColors[3][3]);
}

__interrupt void joy_interrupt()
{
	joy_poll(1);

	CrossX += joyx[1]; CrossY += joyy[1];

	if (CrossX < 8)
		CrossX = 8;
	else if (CrossX > 312)
		CrossX = 312;
	if (CrossY < 16)
		CrossY = 16;
	else if (CrossY > 172)
		CrossY = 172;

	spr_move(0, CrossX + 14, CrossY + 40);	

	if (joyb[1])
	{
		if (CrossDelay == 0)
		{
			CrossP = true;
			CrossDelay = 4;
		}
	}
	else if (CrossDelay > 0)
		CrossDelay--;
}

RIRQCode	bottom, top;

int main(void)
{
	mmap_trampoline();

	mmap_set(MMAP_RAM);
	memcpy(Sprites, MissileSprites, 1024);
	memcpy(Charset, MissileChars, 2048);
	mmap_set(MMAP_NO_ROM);

	vic_setmode(VICM_HIRES_MC, Color1, Hires);
	spr_init(Color1);

	// initialize raster IRQ
	rirq_init(true);

	vic.color_back = VCOL_BLACK;
	vic.color_border = VCOL_BLACK;

	memset(Color1, 0x18, 1000);
	memset(Color2, 0x06, 1000);
	memset(Hires, 0, 8000);
	memset(Color2 + 40 * 23, 0x07, 80);

	bm_init(&sbm, Hires, 40, 25);
	missile_init();
	explosion_init();
	icbm_init();
	mountains_init();

	spr_set(0, true, CrossX + 14, CrossY + 40, 64, 1, false, false, false);

	for(char i=0; i<3; i++)
	{
		spr_set(i + 1, true, 70 + 32 * i, 222, 65, 15, false, false, false);		
		spr_set(i + 4, true, 214 + 32 * i, 222, 65, 15, false, false, false);		

		cities[i + 0] = true;
		cities[i + 3] = true;
	}

	// Build the switch to normal IRQ
	rirq_build(&top, 3);
	// Change color for ceiling
	rirq_delay(&top, 10);
	rirq_write(&top, 1, &vic.ctrl1, VIC_CTRL1_BMM | VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	rirq_write(&top, 2, &vic.memptr, 0x28);
	// place this at the bottom
	rirq_set(0, 57, &top);

	// Build the switch to normal IRQ
	rirq_build(&bottom, 3);
	rirq_write(&bottom, 0, &vic.memptr, 0x27);
	rirq_write(&bottom, 1, &vic.ctrl1, VIC_CTRL1_DEN | VIC_CTRL1_RSEL | 3);
	// Change color for ceiling
	rirq_call(&bottom, 2, joy_interrupt);
	// place this at the bottom
	rirq_set(1, 250, &bottom);

	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	bool	launched = false;
	for(;;)
	{
		if (CrossP)
		{
			int	sx = 160;
			if (CrossX < 120)
				sx = 24;
			else if (CrossX > 200)
				sx = 296
			missile_start(sx, 184, CrossX, CrossY);
			CrossP = false;
		}

		if (!(rand() & 63))
		{	
			char ci;
			do {
				ci = rand() & 7;
			} while (ci >= 6 || !cities[ci]);

			int	cx;
			if (ci < 3)
				cx = 58 + 32 * ci;
			else
				cx = 202 + 32 * (ci - 3);

			icbm_start((rand() & 0xff) + 32, 0, cx, 184);
		}

		for(char i=0; i<4; i++)
			missile_animate();
		icbm_animate();
		explosion_animate();

		vic_waitFrame();
	}

	return 0;
}
