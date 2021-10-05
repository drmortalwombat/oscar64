

void copyi(int * a, int * b, int n)
{
	for(int i=0; i<n; i++)
		b[i] = a[i];	
}

int main(void)
{
	int	t[100], s[100];
	for(int i=0; i<100; i++)
		s[i] = i;
	copyi(s, t, 100);
	int	sum = 0;
	for(int i=0; i<100; i++)
		sum += t[i];
		
	return sum - 4950;
}
