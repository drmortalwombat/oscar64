#ifndef C64_EASYFLASH_H
#define C64_EASYFLASH_H

#include "types.h"

struct EasyFlash
{
	volatile byte	bank;
	byte			pad1;
	volatile byte	control;
};

#define EFCTRL_GAME		0x01
#define EFCTRL_EXROM	0x02
#define EFCTRL_MODE		0x04
#define EFCTRL_LED		0x80


#define eflash	(*(EasyFlash *)0xde00)

#ifdef __cplusplus

#ifdef EFPROX_SECTION
#pragma code(EFPROX_SECTION)
#endif

template<int back, class fn, class ... P>
__noinline auto ef_call_p(P... p)
{
	if (back != __bankof(fn))
		eflash.bank = __bankof(fn);
	auto r = fn(p...);
	if (back != 0xff && back != __bankof(fn))
		eflash.bank = back;
	return r;
}

#ifdef EFPROX_SECTION
#pragma code(code)
#endif

template<class fn>
class EFlashCall
{
public:
	template<class ... P>
	__forceinline auto operator()(P ... p) const
	{
		switch(__bankof(0))
		{
#for(i,64) case i: return ef_call_p<i, fn, P...>(p...);
		default:
			return ef_call_p<0xff, fn, P...>(p...);
		}			
	}
};

#define EF_CALL(fn)	EFlashCall<fn##_p>	fn

#endif

#endif

