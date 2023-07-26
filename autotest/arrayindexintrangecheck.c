int a[10];

int get(int i)
{
	return a[i];
}

void put(int i, int x)
{
	a[i] = x;
}

int main(void)
{
	for(int j=0; j<10; j++)
		put(j, j);
	int s = -45;
	for(int j=0; j<10; j++)
		s += get(j);
	return s;
}

