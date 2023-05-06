#ifndef SETJMP_H
#define SETJMP_H

struct _jmp_buf
{
	void 	* sp, * ip, * fp;
	char	tmps[32];
	char	cpup;
	char	stack[48];
};

typedef struct _jmp_buf jmp_buf[1];

__dynstack int setjmp(jmp_buf env);

__dynstack int setjmpsp(jmp_buf env, void * sp);

__dynstack void longjmp(jmp_buf env, int value);


#pragma compile("setjmp.c")

#endif

