#include "neslib.h"
// NES hardware-dependent functions by Shiru (shiru@mail.ru)
// with improvements by VEG
// Feel free to do anything you want with this code, consider it Public Domain

const char palBrightTable[] = {
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0f,0x0f,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x00,0x00,0x00,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x10,0x10,0x10,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x20,0x20,0x20,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30
};

char OAM_BUF[256];
char PAL_BUF[32];

#pragma align(OAM_BUF, 256)

char NTSC_MODE;
volatile char FRAME_CNT1;
volatile char FRAME_CNT2;
char VRAM_UPDATE;
char *	NAME_UPD_ADR;
char NAME_UPD_ENABLE;
char PAL_UPDATE;
const char *	PAL_BG_PTR;
const char *	PAL_SPR_PTR;
char SCROLL_X;
char SCROLL_Y;
char SCROLL_X1;
char SCROLL_Y1;
char PAD_STATE[2];
char PAD_STATEP[2];
char PAD_STATET[2];
char PPU_CTRL_VAR;
char PPU_CTRL_VAR1;
char PPU_MASK_VAR;
char RAND_SEED[2];

int main(void)
{
	ppu.mask = 0;
	nesio.dmc_freq = 0;
	ppu.control = 0;

	char c = ppu.status;
	do {} while (!(ppu.status & 0x80));
	do {} while (!(ppu.status & 0x80));

	nesio.input[1] = 0x40;

	ppu.addr = 0x3f;
	ppu.addr = 0x00;
	for(char i=0; i<32; i++)
		ppu.data = 0x0f;

	ppu.addr = 0x20;
	ppu.addr = 0x00;

	for(unsigned i=0; i<0x1000; i++)
		ppu.data = 0x00;

	char i = 0;
	do {
		((char *)0x200)[i] = 0;
		((char *)0x300)[i] = 0;
		((char *)0x400)[i] = 0;
		((char *)0x500)[i] = 0;
		((char *)0x600)[i] = 0;
		((char *)0x700)[i] = 0;
		i++;
	} while (i);

	pal_bright(4);
	pal_clear();
	oam_clear();

	PPU_CTRL_VAR = 0x80;
	PPU_MASK_VAR = 0x06;

	ppu.control = 0x80;

	RAND_SEED[0] = 0xfd;
	RAND_SEED[1] = 0xfd;

	ppu.scroll = 0x00;
	ppu.scroll = 0x00;
	ppu.oamaddr = 0x00;

	nes_game();

	return 0;
}

__hwinterrupt void nmi(void)
{
	if (PPU_MASK_VAR & 0x18)
	{
		nesio.oamdma = (unsigned)(&OAM_BUF[0]) >> 8;

		if (PAL_UPDATE)
		{
			PAL_UPDATE = 0;
			ppu.addr = 0x3f;
			ppu.addr = 0x00;

			char c = PAL_BG_PTR[PAL_BUF[0]];

			ppu.data = c;
			ppu.data = PAL_BG_PTR[PAL_BUF[1]];
			ppu.data = PAL_BG_PTR[PAL_BUF[2]];
			ppu.data = PAL_BG_PTR[PAL_BUF[3]];

			#pragma unroll(full)
			for(char j=0; j<3; j++)
			{
				ppu.data = c;
				ppu.data = PAL_BG_PTR[PAL_BUF[5 + 4 * j + 0]];
				ppu.data = PAL_BG_PTR[PAL_BUF[5 + 4 * j + 1]];
				ppu.data = PAL_BG_PTR[PAL_BUF[5 + 4 * j + 2]];
			}

			#pragma unroll(full)
			for(char j=0; j<4; j++)
			{
				ppu.data = c;
				ppu.data = PAL_SPR_PTR[PAL_BUF[17 + 4 * j + 0]];
				ppu.data = PAL_SPR_PTR[PAL_BUF[17 + 4 * j + 1]];
				ppu.data = PAL_SPR_PTR[PAL_BUF[17 + 4 * j + 2]];
			}
		}

		if (VRAM_UPDATE)
		{
			VRAM_UPDATE = 0;

			if (NAME_UPD_ENABLE)
				flush_vram_update(NAME_UPD_ADR);
		}

		ppu.addr = 0x00;
		ppu.addr = 0x00;

		ppu.scroll = SCROLL_X;
		ppu.scroll = SCROLL_Y;

		ppu.control = PPU_CTRL_VAR;
	}

	ppu.mask = PPU_MASK_VAR;

	FRAME_CNT1++;
	FRAME_CNT2++;
	if (FRAME_CNT2 == 6)
		FRAME_CNT2 = 0;

	//	jsr FamiToneUpdate
}

void pal_all(const char *data)
{
	for(char i=0; i<32; i++)
		PAL_BUF[i] = data[i];
	PAL_UPDATE++;
}

void pal_bg(const char *data)
{
	for(char i=0; i<16; i++)
		PAL_BUF[i] = data[i];
	PAL_UPDATE++;
}	

void pal_spr(const char *data)
{
	for(char i=0; i<16; i++)
		PAL_BUF[i + 16] = data[i];
	PAL_UPDATE++;	
}

void pal_col(unsigned char index,unsigned char color)
{
	PAL_BUF[index & 0x1f] = color;
	PAL_UPDATE++;
}

void pal_clear(void)
{
	for(char i=0; i<32; i++)
		PAL_BUF[i] = 0x0f;
	PAL_UPDATE++;
}

void pal_spr_bright(unsigned char bright)
{
	PAL_SPR_PTR = palBrightTable + 16 * bright;
	PAL_UPDATE++;
}

void pal_bg_bright(unsigned char bright)
{
	PAL_BG_PTR = palBrightTable + 16 * bright;
	PAL_UPDATE++;	
}



void pal_bright(unsigned char bright)
{
	pal_spr_bright(bright);
	pal_bg_bright(bright);
}

void ppu_off(void)
{
	PPU_MASK_VAR &= 0b11100111;
	ppu_wait_nmi();
}

void ppu_on_all(void)
{
	PPU_MASK_VAR|= 0b00011000;
	ppu_wait_nmi();	
}

void ppu_on_bg(void)
{
	PPU_MASK_VAR |= 0b00001000;
	ppu_wait_nmi();		
}

void ppu_on_spr(void)
{
	PPU_MASK_VAR |= 0b00010000;
	ppu_wait_nmi();			
}

void ppu_mask(unsigned char mask)
{
	PPU_MASK_VAR = mask;
}

unsigned char ppu_system(void)
{
	return NTSC_MODE;
}

unsigned char get_ppu_ctrl_var(void)
{
	return PPU_CTRL_VAR;
}

void set_ppu_ctrl_var(unsigned char var)
{
	PPU_CTRL_VAR = var;
}

void oam_clear(void)
{
	char i = 0;
	do {
		OAM_BUF[i] = 0;
		i += 4;
	} while (i);
}

void oam_size(unsigned char size)
{
	if (size & 1)
		PPU_CTRL_VAR |= 0x20;
	else
		PPU_CTRL_VAR &= ~0x20;
}

unsigned char oam_spr(unsigned char x,unsigned char y,unsigned char chrnum,unsigned char attr,unsigned char sprid)
{
	OAM_BUF[sprid + 2] = attr;
	OAM_BUF[sprid + 1] = chrnum;
	OAM_BUF[sprid + 0] = y;
	OAM_BUF[sprid + 3] = x;
	return attr + 4;
}

unsigned char oam_meta_spr(unsigned char x,unsigned char y,unsigned char sprid,const unsigned char *data)
{
	char i = 0;
	while (!(data[i] & 0x80))
	{
		OAM_BUF[sprid + 3] = x + data[i + 0];
		OAM_BUF[sprid + 0] = y + data[i + 1];
		OAM_BUF[sprid + 1] = data[i + 2];
		OAM_BUF[sprid + 2] = data[i + 3];

		sprid += 4;
		i += 4;
	}
	return sprid;
}


void oam_hide_rest(unsigned char sprid)
{
	do {
		OAM_BUF[sprid] = 240;
		sprid += 4;
	} while (sprid);
}

void ppu_wait_frame(void)
{
	VRAM_UPDATE = 1;
	char c = FRAME_CNT1;
	while (c == FRAME_CNT1) ;
	if (NTSC_MODE)
	{
		while (FRAME_CNT2 == 5) ;
	}
}

void ppu_wait_nmi(void)
{
	VRAM_UPDATE = 1;
	char c = FRAME_CNT1;
	while (c == FRAME_CNT1) ;	
}

void vram_unrle(const unsigned char *data)
{
	char tag = *data++;
	char b;

	for(;;)
	{
		char c = *data++;
		if (c != tag)
		{
			ppu.data = c;
			b = c;
		}
		else
		{
			c = *data++;
			if (!c)
				return;
			while (c)
			{
				ppu.data = b;
				c--;
			}
		}
	}
}

void scroll(unsigned int x,unsigned int y)
{
	char b = (PPU_CTRL_VAR & 0xfc) | ((x >> 8) & 1);

	if (y >= 240)
	{
		y -= 240;
		b |= 2;
	}

	SCROLL_Y = y;
	SCROLL_X = x;

	PPU_CTRL_VAR = b;
}

void split(unsigned int x,unsigned int y)
{
	char b = (PPU_CTRL_VAR & 0xfc) | ((x >> 8) & 1);

	SCROLL_X1 = x;
	PPU_CTRL_VAR1 = b;

	while (ppu.status & 0x40) ;
	while (!(ppu.status & 0x40)) ;

	ppu.scroll = SCROLL_X1;
	ppu.scroll = 0;
	ppu.control = PPU_CTRL_VAR1;
}

void bank_spr(unsigned char n)
{
	if (n & 1)
		PPU_CTRL_VAR |= 0x08;
	else
		PPU_CTRL_VAR &= ~0x08;
}

void bank_bg(unsigned char n)
{
	if (n & 1)
		PPU_CTRL_VAR |= 0x10;
	else
		PPU_CTRL_VAR &= ~0x10;	
}

void vram_read(unsigned char *dst,unsigned int size)
{
	for(unsigned i=size; i!=0; i--)
		*dst++ = ppu.data;	
}

void vram_write(const unsigned char *src,unsigned int size)
{
	for(unsigned i=size; i!=0; i--)
		ppu.data = *src++;	
}

void music_play(unsigned char song)
{
	//_music_play=FamiToneMusicPlay
}

void music_stop(void)
{
	//_music_stop=FamiToneMusicStop

}

void music_pause(unsigned char pause)
{
	//_music_pause=FamiToneMusicPause
}

void sfx_play(unsigned char sound,unsigned char channel)
{
#if 0
_sfx_play:

.if(FT_SFX_ENABLE)

	and #$03
	tax
	lda @sfxPriority,x
	tax
	jsr popa
	jmp FamiToneSfxPlay

@sfxPriority:

	.byte FT_SFX_CH0,FT_SFX_CH1,FT_SFX_CH2,FT_SFX_CH3

.else
	rts
.endif
#endif
}

void sample_play(unsigned char sample)
{
#if 0
.if(FT_DPCM_ENABLE)
_sample_play=FamiToneSamplePlay
.else
_sample_play:
	rts
.endif
#endif
}

unsigned char pad_poll(unsigned char pad)
{
	char buf[3];

	for(char j=0; j<3; j++)
	{
		nesio.input[0] = 1;
		nesio.input[0] = 0;

		char c = 0;
		for(char i=0; i<8; i++)
		{
			c = (c | (nesio.input[pad] << 8)) >> 1;
		}
		buf[j] = c;
	}

	char b = buf[0];
	if (b != buf[1] && b != buf[2])
		b = buf[1];

	PAD_STATE[pad] = b;
	PAD_STATET[pad] = (b ^ PAD_STATEP[pad]) & PAD_STATE[pad];
	PAD_STATEP[pad] = b;

	return b;
}

unsigned char pad_trigger(unsigned char pad)
{
	pad_poll(pad);
	return PAD_STATET[pad];
}

unsigned char pad_state(unsigned char pad)
{
	return PAD_STATE[pad];
}

unsigned char rand1(void)
{
	if (RAND_SEED[0] & 0x80)
	{
		RAND_SEED[0] <<= 1;
		RAND_SEED[0] ^= 0xcf;
	}
	else
		RAND_SEED[0] <<= 1;
	return RAND_SEED[0];
}


unsigned char rand2(void)
{
	if (RAND_SEED[1] & 0x80)
	{
		RAND_SEED[1] <<= 1;
		RAND_SEED[1] ^= 0xd7;
	}
	else
		RAND_SEED[1] <<= 1;
}

unsigned char rand8(void)
{
	return rand1() + rand2();
}

unsigned int rand16(void)
{
	return (rand1() << 8) | rand2();
}

void set_rand(unsigned seed)
{
	RAND_SEED[0] = seed & 0xff;
	RAND_SEED[1] = seed >> 8;
}

void set_vram_update(unsigned char *buf)
{
	NAME_UPD_ADR = buf;
	NAME_UPD_ENABLE = buf != nullptr;
}

void flush_vram_update(unsigned char *buf)
{
	char i = 0;
	for(;;)
	{
		char c = buf[i++];
		if (c < 0x40)
		{
			ppu.addr = c;
			ppu.addr = buf[i++];
			ppu.data = buf[i++];
		}
		else
		{
			if (c < 0x80)
				ppu.control = PPU_CTRL_VAR | 0x04;
			else if (c != 0xff)
				ppu.control = PPU_CTRL_VAR & ~0x04;
			else
				return;

			ppu.addr = c & 0x3f;
			ppu.addr = buf[i++];
			c = buf[i++];
			do 	{
				ppu.data = buf[i++];
				c--;
			} while (c);
			ppu.control = PPU_CTRL_VAR;
		}
	}
}

void vram_adr(unsigned int addr)
{
	ppu.addr = addr >> 8;
	ppu.addr = addr & 0xff;	
}

void vram_put(unsigned char n)
{
	ppu.data = n;
}

void vram_fill(unsigned char n,unsigned int size)
{
	for(unsigned i=size; i!=0; i--)
		ppu.data = n;		
}

void vram_inc(unsigned char n)
{
	if (n)
		PPU_CTRL_VAR |= 0x04;
	else
		PPU_CTRL_VAR &= ~0x04;
	ppu.control = PPU_CTRL_VAR;
}

void memfill(void *dst,unsigned char value,unsigned int size)
{
	for(unsigned i=size; i!=0; i--)
		*dst++ = value;	
}

unsigned char nesclock(void)
{
	return FRAME_CNT1;
}

void delay(unsigned char frames)
{
	while (frames)
	{
		ppu_wait_nmi();
		frames--;
	}
}

#pragma data(boot)

__export struct Boot
{
	void * nmi, * reset, * irq;
}	boot = {
	nmi,
	(void *)0x8000,
	nullptr
};

#pragma data(data)

