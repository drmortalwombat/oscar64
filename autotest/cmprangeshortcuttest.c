#include <assert.h>

void check_s_lt(void)
{
	int	a5 = 0, a10 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i < 5)
			a5++;
		if (i < 10)
			a10++;
		if (i < 15)
			a15++;
	}
	
	assert(a5 == 0);
	assert(a10 == 5);
	assert(a15 == 10);
}

void check_s_le(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i <= 4)
			a4++;
		if (i <= 5)
			a5++;
		if (i <= 10)
			a10++;
		if (i <= 14)
			a14++;
		if (i <= 15)
			a15++;
	}
	
	assert(a4 == 0);
	assert(a5 == 1);
	assert(a10 == 6);
	assert(a14 == 10);
	assert(a15 == 10);
}

void check_s_ge(void)
{
	int	a5 = 0, a10 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i >= 5)
			a5++;
		if (i >= 10)
			a10++;
		if (i >= 15)
			a15++;
	}
	
	assert(a5 == 10);
	assert(a10 == 5);
	assert(a15 == 0);
}

void check_s_gt(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i > 4)
			a4++;
		if (i > 5)
			a5++;
		if (i > 10)
			a10++;
		if (i > 14)
			a14++;
		if (i > 15)
			a15++;
	}
	
	assert(a4 == 10);
	assert(a5 ==  9);
	assert(a10 == 4);
	assert(a14 == 0);
	assert(a15 == 0);
}

void check_s_eq(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i == 4)
			a4++;
		if (i == 5)
			a5++;
		if (i == 10)
			a10++;
		if (i == 14)
			a14++;
		if (i == 15)
			a15++;
	}
	
	assert(a4 == 0);
	assert(a5 ==  1);
	assert(a10 == 1);
	assert(a14 == 1);
	assert(a15 == 0);
}

void check_s_ne(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(int i=5; i<15; i++)
	{
		if (i != 4)
			a4++;
		if (i != 5)
			a5++;
		if (i != 10)
			a10++;
		if (i != 14)
			a14++;
		if (i != 15)
			a15++;
	}
	
	assert(a4 == 10);
	assert(a5 ==  9);
	assert(a10 == 9);
	assert(a14 == 9);
	assert(a15 == 10);
}

void check_u_lt(void)
{
	int	a5 = 0, a10 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i < 5)
			a5++;
		if (i < 10)
			a10++;
		if (i < 15)
			a15++;
	}
	
	assert(a5 == 0);
	assert(a10 == 5);
	assert(a15 == 10);
}

void check_u_le(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i <= 4)
			a4++;
		if (i <= 5)
			a5++;
		if (i <= 10)
			a10++;
		if (i <= 14)
			a14++;
		if (i <= 15)
			a15++;
	}
	
	assert(a4 == 0);
	assert(a5 == 1);
	assert(a10 == 6);
	assert(a14 == 10);
	assert(a15 == 10);
}

void check_u_ge(void)
{
	int	a5 = 0, a10 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i >= 5)
			a5++;
		if (i >= 10)
			a10++;
		if (i >= 15)
			a15++;
	}
	
	assert(a5 == 10);
	assert(a10 == 5);
	assert(a15 == 0);
}

void check_u_gt(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i > 4)
			a4++;
		if (i > 5)
			a5++;
		if (i > 10)
			a10++;
		if (i > 14)
			a14++;
		if (i > 15)
			a15++;
	}
	
	assert(a4 == 10);
	assert(a5 ==  9);
	assert(a10 == 4);
	assert(a14 == 0);
	assert(a15 == 0);
}

void check_u_eq(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i == 4)
			a4++;
		if (i == 5)
			a5++;
		if (i == 10)
			a10++;
		if (i == 14)
			a14++;
		if (i == 15)
			a15++;
	}
	
	assert(a4 == 0);
	assert(a5 ==  1);
	assert(a10 == 1);
	assert(a14 == 1);
	assert(a15 == 0);
}

void check_u_ne(void)
{
	int	a4 = 0, a5 = 0, a10 = 0, a14 = 0, a15 = 0;
	
	for(unsigned i=5; i<15; i++)
	{
		if (i != 4)
			a4++;
		if (i != 5)
			a5++;
		if (i != 10)
			a10++;
		if (i != 14)
			a14++;
		if (i != 15)
			a15++;
	}
	
	assert(a4 == 10);
	assert(a5 ==  9);
	assert(a10 == 9);
	assert(a14 == 9);
	assert(a15 == 10);
}


int main(void)
{
	check_s_ne();
	check_s_eq();
	check_s_lt();
	check_s_le();
	check_s_gt();
	check_s_ge();
	
	check_u_ne();
	check_u_eq();
	check_u_lt();
	check_u_le();
	check_u_gt();
	check_u_ge();
	
	return 0;
}

