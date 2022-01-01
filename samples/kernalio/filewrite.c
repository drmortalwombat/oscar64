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
	// Set name for file and open it with replace on drive 9
	krnio_setnam("@0:HIGHSCORE,P,W");
	if (krnio_open(2, 9, 2))
	{
		// Fill the file with the score array
		krnio_write(2, (char*)score, sizeof(score));

		// Close the file
		krnio_close(2);
	}

	return 0;
}
