#include <stdio.h>
#include <assert.h>

typedef signed char int8;

bool beq(int8 a, int8 b)
{
	return a == b;
}

bool blt(int8 a, int8 b)
{
	return a < b;
}

bool bgt(int8 a, int8 b)
{
	return a > b;
}

bool ble(int8 a, int8 b)
{
	return a <= b;
}

bool bge(int8 a, int8 b)
{
	return a >= b;
}

bool neq(int8 a, int8 b)
{
	return a == b;
}

#pragma native(neq)

bool nlt(int8 a, int8 b)
{
	return a < b;
}

#pragma native(nlt)

bool ngt(int8 a, int8 b)
{
	return a > b;
}

#pragma native(ngt)

bool nle(int8 a, int8 b)
{
	return a <= b;
}

#pragma native(nle)

bool nge(int8 a, int8 b)
{
	return a >= b;
}

#pragma native(nge)



bool beqz(int8 a)
{
	return a == 0;
}

bool bltz(int8 a)
{
	return a < 0;
}

bool bgtz(int8 a)
{
	return a > 0;
}

bool blez(int8 a)
{
	return a <= 0;
}

bool bgez(int8 a)
{
	return a >= 0;
}

bool neqz(int8 a)
{
	return a == 0;
}

#pragma native(neqz)

bool nltz(int8 a)
{
	return a < 0;
}

#pragma native(nltz)

bool ngtz(int8 a)
{
	return a > 0;
}

#pragma native(ngtz)

bool nlez(int8 a)
{
	return a <= 0;
}

#pragma native(nlez)

bool ngez(int8 a)
{
	return a >= 0;
}

#pragma native(ngez)




bool beq1(int8 a)
{
	return a == 1;
}

bool blt1(int8 a)
{
	return a < 1;
}

bool bgt1(int8 a)
{
	return a > 1;
}

bool ble1(int8 a)
{
	return a <= 1;
}

bool bge1(int8 a)
{
	return a >= 1;
}

bool neq1(int8 a)
{
	return a == 1;
}

#pragma native(neq1)

bool nlt1(int8 a)
{
	return a < 1;
}

#pragma native(nlt1)

bool ngt1(int8 a)
{
	return a > 1;
}

#pragma native(ngt1)

bool nle1(int8 a)
{
	return a <= 1;
}

#pragma native(nle1)

bool nge1(int8 a)
{
	return a >= 1;
}

#pragma native(nge1)



void cmp(int8 a, int8 b)
{
	bool	beqf = beq(a, b), bltf = blt(a, b), bgtf = bgt(a, b), blef = ble(a, b), bgef = bge(a, b);
	bool	neqf = neq(a, b), nltf = nlt(a, b), ngtf = ngt(a, b), nlef = nle(a, b), ngef = nge(a, b);
	
	printf("BYTE   %d, %d : EQ %d LT %d GT %d\r", a, b, beqf, bltf, bgtf);
	printf("NATIVE %d, %d : EQ %d LT %d GT %d\r", a, b, neqf, nltf, ngtf);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

void cmpz(int8 a)
{
	bool	beqf = beqz(a), bltf = bltz(a), bgtf = bgtz(a), blef = blez(a), bgef = bgez(a);
	bool	neqf = neqz(a), nltf = nltz(a), ngtf = ngtz(a), nlef = nlez(a), ngef = ngez(a);
	
	printf("BYTE   %d, 0 : EQ %d LT %d GT %d\r", a, beqf, bltf, bgtf);
	printf("NATIVE %d, 0 : EQ %d LT %d GT %d\r", a, neqf, nltf, ngtf);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

void cmp1(int8 a)
{
	bool	beqf = beq1(a), bltf = blt1(a), bgtf = bgt1(a), blef = ble1(a), bgef = bge1(a);
	bool	neqf = neq1(a), nltf = nlt1(a), ngtf = ngt1(a), nlef = nle1(a), ngef = nge1(a);
	
	printf("BYTE   %d, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, beqf, bltf, bgtf, blef, bgef);
	printf("NATIVE %d, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, neqf, nltf, ngtf, nlef, ngef);
	
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


	cmp( 0,  100);
	cmp( 0, -100);
	cmp( 100,  0);
	cmp(-100,  0);

	cmp(10, 10);
	cmp(10, 20);
	cmp(20, 10);

	cmp(-10, -10);
	cmp(-10, -20);
	cmp(-20, -10);
	
	cmp( 10, -10);
	cmp( 10, -20);
	cmp( 20, -10);

	cmp(-10,  10);
	cmp(-10,  20);
	cmp(-20,  10);

	cmp(-30,  30);
	cmp(-30, -30);
	cmp( 30,  30);
	cmp( 30, -30);
	
	cmp( 0,  127);
	cmp( 0, -128);
	cmp( 127,  0);
	cmp(-128,  0);

	cmp( 127,  127);
	cmp( 127, -128);
	cmp(-128,  127);
	cmp(-128, -128);

	cmpz(0);
	cmpz(1);
	cmpz(127);
	cmpz(-1);
	cmpz(-128);

	cmp1(0);
	cmp1(1);
	cmp1(2);
	cmp1(3);
	cmp1(127);
	cmp1(-1);
	cmp1(-2);
	cmp1(-3);
	cmp1(-128);

	return 0;
	
}
