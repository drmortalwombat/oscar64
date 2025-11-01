#include <assert.h>

void unroll1(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<100; i++)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<100; i++)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<100; i++)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<100; i++)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<100; i++)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unroll2(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<100; i+=2)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<100; i+=2)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<100; i+=2)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<100; i+=2)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<100; i+=2)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unroll3(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<100; i+=3)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<100; i+=3)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<100; i+=3)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<100; i+=3)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<100; i+=3)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unroll50(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<100; i+=50)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<100; i+=50)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<100; i+=50)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<100; i+=50)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<100; i+=50)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unroll50e(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<=100; i+=50)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<=100; i+=50)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<=100; i+=50)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<=100; i+=50)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<=100; i+=50)
		sumfull += i;

	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unroll100(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=0; i<100; i+=100)
		sum0 += i;
	#pragma unroll(2)
	for(int i=0; i<100; i+=100)
		sum2 += i;
	#pragma unroll(3)
	for(int i=0; i<100; i+=100)
		sum3 += i;
	#pragma unroll(10)
	for(int i=0; i<100; i+=100)
		sum10 += i;
	#pragma unroll(full)
	for(int i=0; i<100; i+=100)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}


void unrolld1(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>0; i--)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>0; i--)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>0; i--)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>0; i--)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>0; i--)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unrolld2(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>0; i-=2)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>0; i-=2)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>0; i-=2)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>0; i-=2)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>0; i-=2)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unrolld3(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>0; i-=3)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>0; i-=3)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>0; i-=3)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>0; i-=3)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>0; i-=3)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unrolld50(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>0; i-=50)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>0; i-=50)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>0; i-=50)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>0; i-=50)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>0; i-=50)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unrolld50e(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>=0; i-=50)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>=0; i-=50)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>=0; i-=50)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>=0; i-=50)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>=0; i-=50)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}

void unrolld100(void)
{
	int sum0 = 0;
	int sum2 = 0;
	int sum3 = 0;
	int sum10 = 0;
	int sumfull = 0;

	for(int i=100; i>0; i-=100)
		sum0 += i;
	#pragma unroll(2)
	for(int i=100; i>0; i-=100)
		sum2 += i;
	#pragma unroll(3)
	for(int i=100; i>0; i-=100)
		sum3 += i;
	#pragma unroll(10)
	for(int i=100; i>0; i-=100)
		sum10 += i;
	#pragma unroll(full)
	for(int i=100; i>0; i-=100)
		sumfull += i;
	assert(sum2 == sum0);
	assert(sum3 == sum0);
	assert(sum10 == sum0);
	assert(sumfull == sum0);
}


char glob[20];

void autounroll(void)
{
	char sum0 = 0;
	char sum1 = 0;
	char sum2 = 0;
	char sum3 = 0;
	char sum4 = 0;

	for(int i=0; i<10; i++)
		sum0 += glob[i];
//	for(int i=10; i<20; i++)
//		sum1 += glob[i - 10];
	for(int i=10; i>0; i--)
		sum2 += glob[i - 1];
	for(int i=9; i>=0; i--)
		sum3 += glob[i];
//	for(int i=0; i<=9; i++)
//		sum4 += glob[i];

//	assert(sum1 == sum0);
	assert(sum2 == sum0);
#if 0
	assert(sum3 == sum0);
	assert(sum4 == sum0);
#endif
}

int main(void)
{
	for(int i=0; i<20; i++)
		glob[i] = i;
#if 0
	unroll1();
	unroll2();
	unroll3();
	unroll50();
	unroll50e();
	unroll100();
	unrolld1();
	unrolld2();
	unrolld3();
	unrolld50();
	unrolld50e();
	unrolld100();
#endif
	autounroll();
}
