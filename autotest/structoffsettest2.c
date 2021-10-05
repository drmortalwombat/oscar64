struct A
{
	int x;
	struct B
	{
		int m;
		struct C
		{
			int w;
		}	c;
	}	b;
}	q;

int test(A * a)
{
	a->b.c.w = 1;
	return a->b.c.w;
}

int main(void)
{
	return test(&q) - 1;
}

	
