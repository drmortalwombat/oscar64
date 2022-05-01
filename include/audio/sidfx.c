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

static struct SIDFXChannel
{
	SIDFX	*	com;
	byte		delay, cnt;
	SIDFXState	state;
	unsigned	freq, pwm;

}	channels[3];

void sidfx_init(void)
{
	for(char i=0; i<3; i++)
	{
		channels[i].com = nullptr;
		channels[i].state = SIDFX_IDLE;
	}
}

void sidfx_play(byte chn, SIDFX * fx, byte cnt)
{
	if (!channels[chn].com || channels[chn].com->priority <= fx->priority)
	{
		if (channels[chn].state == SIDFX_IDLE)
			channels[chn].state = SIDFX_READY;
		else
			channels[chn].state = SIDFX_RESET_0;

		channels[chn].com = fx;
		channels[chn].cnt = cnt - 1;
	}
}

void sidfx_stop(byte chn)
{
	channels[chn].com = nullptr;
	if (channels[chn].state != SIDFX_IDLE)
		channels[chn].state = SIDFX_RESET_0;
}

inline void sidfx_loop_ch(byte ch)
{
	switch (channels[ch].state)
	{
		case SIDFX_IDLE:
			break;
		case SIDFX_RESET_0:
			sid.voices[ch].ctrl = 0;
			sid.voices[ch].attdec = 0;
			sid.voices[ch].susrel = 0;
			channels[ch].state = SIDFX_RESET_1;
			break;
		case SIDFX_RESET_1:
			sid.voices[ch].ctrl = SID_CTRL_TEST;
			channels[ch].state = SIDFX_READY;
			break;
		case SIDFX_READY:
			if (channels[ch].com)
			{
				channels[ch].freq = channels[ch].com->freq;
				channels[ch].pwm = channels[ch].com->pwm;

				sid.voices[ch].freq = channels[ch].com->freq;
				sid.voices[ch].pwm = channels[ch].com->pwm;
				sid.voices[ch].attdec = channels[ch].com->attdec;
				sid.voices[ch].susrel = channels[ch].com->susrel;
				sid.voices[ch].ctrl = channels[ch].com->ctrl;

				channels[ch].delay = channels[ch].com->time1;
				channels[ch].state = SIDFX_PLAY;
			}
			else
				channels[ch].state = SIDFX_IDLE;
			break;
		case SIDFX_PLAY:
			if (channels[ch].com->dfreq)
			{
				channels[ch].freq += channels[ch].com->dfreq;
				sid.voices[ch].freq = channels[ch].freq;
			}
			if (channels[ch].com->dpwm)
			{
				channels[ch].pwm += channels[ch].com->dpwm;
				sid.voices[ch].pwm = channels[ch].pwm;
			}

			if (channels[ch].delay)
				channels[ch].delay--;
			else if (channels[ch].com->time0)
			{
				sid.voices[ch].ctrl = channels[ch].com->ctrl & ~SID_CTRL_GATE;
				channels[ch].delay = channels[ch].com->time0;
				channels[ch].state = SIDFX_WAIT;
			}
			else if (channels[ch].cnt)
			{
				channels[ch].cnt--;
				channels[ch].com++;
				channels[ch].state = SIDFX_READY;
			}
			else
			{
				channels[ch].com = nullptr;
				channels[ch].state = SIDFX_RESET_0;						
			}
			break;
		case SIDFX_WAIT:
			if (channels[ch].com->dfreq)
			{
				channels[ch].freq += channels[ch].com->dfreq;
				sid.voices[ch].freq = channels[ch].freq;
			}
			if (channels[ch].com->dpwm)
			{
				channels[ch].pwm += channels[ch].com->dpwm;
				sid.voices[ch].pwm = channels[ch].pwm;
			}

			if (channels[ch].delay)
				channels[ch].delay--;
			else if (channels[ch].cnt)
			{
				channels[ch].cnt--;
				channels[ch].com++;
				channels[ch].state = SIDFX_RESET_0;
			}
			else
			{
				channels[ch].com = nullptr;
				channels[ch].state = SIDFX_RESET_0;					
			}
			break;
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
