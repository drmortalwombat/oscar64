#include <stdio.h>
#include <assert.h>

bool beq(int a, int b)
{
	return a == b;
}

bool blt(int a, int b)
{
	return a < b;
}

bool bgt(int a, int b)
{
	return a > b;
}

bool ble(int a, int b)
{
	return a <= b;
}

bool bge(int a, int b)
{
	return a >= b;
}

bool neq(int a, int b)
{
	return a == b;
}

#pragma native(neq)

bool nlt(int a, int b)
{
	return a < b;
}

#pragma native(nlt)

bool ngt(int a, int b)
{
	return a > b;
}

#pragma native(ngt)

bool nle(int a, int b)
{
	return a <= b;
}

#pragma native(nlt)

bool nge(int a, int b)
{
	return a >= b;
}

#pragma native(ngt)

void cmp(int a, int b)
{
	bool	beqf = beq(a, b), bltf = blt(a, b), bgtf = bgt(a, b), blef = blt(a, b), bgef = bgt(a, b);
	bool	neqf = neq(a, b), nltf = nlt(a, b), ngtf = ngt(a, b), nlef = nlt(a, b), ngef = ngt(a, b);
	
//	printf("BYTE   %d, %d : EQ %d LT %d GT %d\r", a, b, beqf, bltf, bgtf);
//	printf("NATIVE %d, %d : EQ %d LT %d GT %d\r", a, b, neqf, nltf, ngtf);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

int main(void)
{
	cmp( 0,  1);
	cmp( 0, -1);
	cmp( 1,  0);
	cmp(-1,  0);

	cmp(1, 1);
	cmp(1, 2);
	cmp(2, 1);

	cmp(-1, -1);
	cmp(-1, -2);
	cmp(-2, -1);
	
	cmp( 1, -1);
	cmp( 1, -2);
	cmp( 2, -1);

	cmp(-1,  1);
	cmp(-1,  2);
	cmp(-2,  1);


	cmp( 0,  10000);
	cmp( 0, -10000);
	cmp( 10000,  0);
	cmp(-10000,  0);

	cmp(10000, 10000);
	cmp(10000, 20000);
	cmp(20000, 10000);

	cmp(-10000, -10000);
	cmp(-10000, -20000);
	cmp(-20000, -10000);
	
	cmp( 10000, -10000);
	cmp( 10000, -20000);
	cmp( 20000, -10000);

	cmp(-10000,  10000);
	cmp(-10000,  20000);
	cmp(-20000,  10000);
	
	cmp( 0,  1024);
	cmp( 0, -1024);
	cmp( 1024,  0);
	cmp(-1024,  0);

	cmp(1024, 1024);
	cmp(1024, 1025);
	cmp(1025, 1024);

	cmp(-1024, -1024);
	cmp(-1024, -1025);
	cmp(-1025, -1024);
	
	cmp( 1024, -1024);
	cmp( 1024, -1025);
	cmp( 1025, -1024);

	cmp(-1024,  1024);
	cmp(-1024,  1025);
	cmp(-1025,  1024);

	return 0;
	
}
