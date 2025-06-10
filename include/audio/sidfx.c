#include "sidfx.h"

enum SIDFXState
{
	SIDFX_IDLE,
	SIDFX_RESET_0,
	SIDFX_RESET_1,
	SIDFX_READY,
	SIDFX_PLAY,
	SIDFX_WAIT
};

__striped static struct SIDFXChannel
{
	const SIDFX	* volatile 		com;
	byte						delay, priority;
	volatile byte				cnt;
	volatile SIDFXState			state;
	unsigned					freq, pwm;

}	channels[3];

void sidfx_init(void)
{
	for(char i=0; i<3; i++)
	{
		channels[i].com = nullptr;
		channels[i].state = SIDFX_IDLE;
		channels[i].priority = 0;
		channels[i].delay = 1;
	}
}

bool sidfx_idle(byte chn)
{
	return channels[chn].state == SIDFX_IDLE;
}

char sidfx_cnt(byte chn)
{
	return channels[chn].cnt;	
}

void sidfx_play(byte chn, const SIDFX * fx, byte cnt)
{
	SIDFXState		ns = channels[chn].state;

	if (ns == SIDFX_IDLE)
		ns = SIDFX_READY;
	else if (channels[chn].priority <= fx->priority)
		ns = SIDFX_RESET_0;
	else
		return;

	channels[chn].state = SIDFX_IDLE;
	channels[chn].delay = 1;

	channels[chn].com = fx;
	channels[chn].cnt = cnt - 1;
	channels[chn].priority = fx->priority;

	channels[chn].state = ns;
}

void sidfx_stop(byte chn)
{
	channels[chn].com = nullptr;
	if (channels[chn].state != SIDFX_IDLE)
	{
		channels[chn].state = SIDFX_RESET_0;
		channels[chn].delay = 1;
	}
}

inline void sidfx_loop_ch(byte ch)
{
	if (channels[ch].state)
	{
		const SIDFX	*	com = channels[ch].com;

		channels[ch].delay--;
		if (channels[ch].delay)
		{
			if (com->dfreq)
			{
				channels[ch].freq += com->dfreq;
				sid.voices[ch].freq = channels[ch].freq;
			}
			if (com->dpwm)
			{
				channels[ch].pwm += com->dpwm;
				sid.voices[ch].pwm = channels[ch].pwm;
			}
		}

		while (!channels[ch].delay)
		{
			switch (channels[ch].state)
			{
			case SIDFX_IDLE:
				channels[ch].delay = 1;
				break;
			case SIDFX_RESET_0:
				sid.voices[ch].ctrl = 0;
				sid.voices[ch].attdec = 0;
				sid.voices[ch].susrel = 0;
				if (com)
					channels[ch].state = SIDFX_READY;
				else
					channels[ch].state = SIDFX_IDLE;
				channels[ch].delay = 1;
				break;
			case SIDFX_RESET_1:
				sid.voices[ch].ctrl = SID_CTRL_TEST;
				sid.voices[ch].ctrl = 0;
				sid.voices[ch].attdec = 0;
				sid.voices[ch].susrel = 0;
				channels[ch].state = SIDFX_READY;
				break;
			case SIDFX_READY:
				channels[ch].freq = com->freq;
				channels[ch].pwm = com->pwm;

				sid.voices[ch].freq = com->freq;
				sid.voices[ch].pwm = com->pwm;
				sid.voices[ch].attdec = com->attdec;
				sid.voices[ch].susrel = com->susrel;
				sid.voices[ch].ctrl = com->ctrl;

				if (com->ctrl & SID_CTRL_GATE)
				{
					channels[ch].delay = com->time1;
					channels[ch].state = SIDFX_PLAY;
				}
				else
				{
					channels[ch].delay = com->time0;
					channels[ch].state = SIDFX_PLAY;
				}
				break;
			case SIDFX_PLAY:
				if (com->time0)
				{
					sid.voices[ch].ctrl = com->ctrl & ~SID_CTRL_GATE;
					channels[ch].delay = com->time0 - 1;
					channels[ch].state = SIDFX_WAIT;
				}
				else if (channels[ch].cnt)
				{
					char sr = com->susrel & 0xf0;
					com++;
					char ctrl = com->ctrl;
					if ((com->attdec & 0xef) == 0 && (ctrl & SID_CTRL_GATE) && (com->susrel & 0xf0) > sr)
					{
						sid.voices[ch].ctrl = ctrl & ~SID_CTRL_GATE;
						sid.voices[ch].ctrl = ctrl |  SID_CTRL_GATE;
					}
					channels[ch].cnt--;
					channels[ch].com = com;
					channels[ch].priority = com->priority;
					channels[ch].state = SIDFX_READY;
				}
				else
				{
					com = nullptr;
					channels[ch].state = SIDFX_RESET_0;					
				}
				break;
			case SIDFX_WAIT:
				if (channels[ch].cnt)
				{
					com++;
					channels[ch].cnt--;
					channels[ch].com = com;
					channels[ch].priority = com->priority;
					if (com->ctrl & SID_CTRL_GATE)
						channels[ch].state = SIDFX_RESET_0;
					else
						channels[ch].state = SIDFX_READY;
				}
				else
				{
					com = nullptr;
					channels[ch].state = SIDFX_RESET_0;					
				}
				break;
			}
		}
	}
}

void sidfx_loop_2(void)
{
	sidfx_loop_ch(2);	
}

void sidfx_loop(void)
{
	for(byte ch=0; ch<3; ch++)
		sidfx_loop_ch(ch);
}	
