

void incv(int * a, int n)
{
	for(int i=0; i<n; i++)
		a[i] ++;		
}

int	t[100];

int main(void)
{
	for(int i=0; i<100; i++)
		t[i] = i;
	incv(t, 100);
	int	s = 0;
	for(int i=0; i<100; i++)
		s += t[i];
		
	return s - 5050;	
}

