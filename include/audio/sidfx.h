#ifndef SIDFX_H
#define SIDFX_H

#include <c64/sid.h>

struct SIDFX
{
	unsigned	freq, pwm;
	byte		ctrl, attdec, susrel;
	int			dfreq, dpwm;
	byte		time1, time0;
	byte		priority;
};

void sidfx_init(void);

inline bool sidfx_idle(byte chn);

inline void sidfx_play(byte chn, const SIDFX * fx, byte cnt);

void sidfx_stop(byte chn);

void sidfx_loop(void);

void sidfx_loop_2(void);

#pragma compile("sidfx.c")

#endif
