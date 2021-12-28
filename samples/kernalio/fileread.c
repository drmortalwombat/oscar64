#include <stdio.h>
#include <c64/kernalio.h>

struct Score
{
	char		name[5];
	unsigned	score;
};

Score	score[4];

int main(void)
{
	krnio_setnam("HIGHSCORE,P,R");	
	if (krnio_open(2, 9, 2))
	{
		krnio_read(2, (char*)score, sizeof(score));

		krnio_close(2);
	}

	for(int i=0; i<4; i++)
	{
		printf("%s : %u\n", score[i].name, score[i].score);
	}

	return 0;
}
