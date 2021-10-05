

int a(int * p, int x)
{
	int y = x + 3;
	
	p[y] = 1;
	p[y + 1] = 2;
	p[y + 2] = 3;
	p[y + 3] = 4;
	
	return p[y] + p[y + 1] + p[y + 2] + p[y + 3];	
}

int main(void)
{
	int	t[16];
	
	return a(t, 4) - 10;
}
