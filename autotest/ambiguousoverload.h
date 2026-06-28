#pragma once

typedef void (*Callback)(void);

struct Holder
{
	int value;
	Callback callback;
};

struct CallbackHolder
{
	Callback callback;
};

void clearView(void);

extern int clearViewCalls;

#pragma compile("ambiguousoverload_trigger.c")
