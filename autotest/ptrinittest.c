#if 1

const struct A {
	char	w;
	int		b[5];
	struct {
		int		c[5];
	}	o;
}	a = {22,
	{4, 5, 6, 7, 8},
	{
		{4, 5, 6, 7, 8}
	}
}

const int * t[4] = {
	a.b + 1 + 1
};

const int * v[4] = {
	a.o.c + 1 + 1
};

int q[5] = {4, 5, 6, 7, 8};

const int * u[4] = {
	q + 2
};

int main(void)
{
	return
		u[0][0] + (q + 2)[2] - 6 - 8 +
		v[0][0] + (a.o.c + 2)[2] - 6 - 8 +
		t[0][0] + (a.b + 2)[2] - 6 - 8;
}

