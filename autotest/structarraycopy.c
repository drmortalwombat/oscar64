

struct Point
{
	int x, y;	
};


Point	tcorners[8], pcorners[8];

int bm_line(void)
{

}

void drawCube(void)
{
	for(char i=0; i<8; i++)
	{
		if (!(i & 1))
			bm_line();
		if (!(i & 2))
			bm_line();
		if (!(i & 4))
			bm_line();

		pcorners[i] = tcorners[i];
	}
}

int main(void)
{
	for(int i=0; i<8; i++)
	{
		tcorners[i].x = (i + 1) * 3;
		tcorners[i].y = (i + 1) * 7;
	}

	drawCube();

	int	sum = 0;
	for(int i=0; i<8; i++)
	{
		sum += pcorners[i].x;
		sum -= tcorners[i].y;
	}

	return sum + 144;
}