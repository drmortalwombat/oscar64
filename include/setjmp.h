#ifndef SETJMP_H
#define SETJMP_H

struct _jmp_buf
{
	void 	* sp, * ip, * fp;
	char	tmps[32];
	char	cpup, cpul, cpuh;
};

typedef struct _jmp_buf jmp_buf[1];

int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int value);


#pragma compile("setjmp.c")

#endif

