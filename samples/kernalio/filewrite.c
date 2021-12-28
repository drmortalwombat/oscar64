#include <stdio.h>
#include <c64/kernalio.h>

struct Score
{
	char		name[5];
	unsigned	score;
};

Score	score[] = {
	{"AAA", 10000},
	{"BBB", 9000},
	{"CCC", 8000},
	{"DDD", 4000}
};

int main(void)
{
	krnio_setnam("@0:HIGHSCORE,P,W");	
	if (krnio_open(2, 9, 2))
	{
		krnio_write(2, (char*)score, sizeof(score));

		krnio_close(2);
	}

	return 0;
}
