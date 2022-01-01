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
	// Set name for file and open it on drive 9
	krnio_setnam("HIGHSCORE,P,R");	
	if (krnio_open(2, 9, 2))
	{
		// Read the content of the file into the score arrayx
		krnio_read(2, (char*)score, sizeof(score));

		// Close the file
		krnio_close(2);
	}

	// Print the result to stdout
	for(int i=0; i<4; i++)
	{
		printf("%s : %u\n", score[i].name, score[i].score);
	}

	return 0;
}
