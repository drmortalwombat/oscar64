
int a(char p[100])
{
	int s = 0;
	for(int i=0; i<100; i++)
		s += p[i];
	return s;
}

int main(void)
{
	char c[100];
	for(int i=0; i<100; i++)
		c[i] = i;
	return a(c) - 4950;
}
