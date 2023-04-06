
int val(char * c)
{
	return c[0];
}

void set(char * c, int a)
{
	c[0] = a;
}

int main(void)
{
	int	sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;

	for(int i=0; i<10; i++)
	{
		char	t[10] = {1};
		sum0 += val(t);
		sum2 += t[0];
		set(t, i);
		sum1 += val(t);
		sum3 += t[0];
	}

	return (sum1 - 45) | (sum0 - 10) | (sum3 - 45) | (sum2 - 10);
}
