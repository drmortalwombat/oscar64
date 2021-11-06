#include <stdio.h>
#include <assert.h>

typedef signed char int8;
typedef unsigned char uint8;

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







bool bequz(uint8 a)
{
	return a == 0;
}

bool bltuz(uint8 a)
{
	return a < 0;
}

bool bgtuz(uint8 a)
{
	return a > 0;
}

bool bleuz(uint8 a)
{
	return a <= 0;
}

bool bgeuz(uint8 a)
{
	return a >= 0;
}

bool nequz(uint8 a)
{
	return a == 0;
}

#pragma native(nequz)

bool nltuz(uint8 a)
{
	return a < 0;
}

#pragma native(nltuz)

bool ngtuz(uint8 a)
{
	return a > 0;
}

#pragma native(ngtuz)

bool nleuz(uint8 a)
{
	return a <= 0;
}

#pragma native(nleuz)

bool ngeuz(uint8 a)
{
	return a >= 0;
}

#pragma native(ngeuz)





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

void cmpuz(uint8 a)
{
	bool	beqf = bequz(a), bltf = bltuz(a), bgtf = bgtuz(a), blef = bleuz(a), bgef = bgeuz(a);
	bool	neqf = nequz(a), nltf = nltuz(a), ngtf = ngtuz(a), nlef = nleuz(a), ngef = ngeuz(a);
	
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

void cmpu(char a, char b, bool lt, bool gt, bool eq)
{
	bool clt = a < b, cgt = a > b, ceq = a == b;
	bool nlt = a >= b, ngt = a <= b, neq = a != b;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", a, b, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpu100(char a, bool lt, bool gt, bool eq)
{
	bool clt = a < 100, cgt = a > 100, ceq = a == 100;
	bool nlt = a >= 100, ngt = a <= 100, neq = a != 100;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", a, 100, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpu100r(char b, bool lt, bool gt, bool eq)
{
	bool clt = 100 < b, cgt = 100 > b, ceq = 100 == b;
	bool nlt = 100 >= b, ngt = 100 <= b, neq = 100 != b;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", 100, b, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpu1000(char a, bool lt, bool gt, bool eq)
{
	bool clt = a < 1000, cgt = a > 1000, ceq = a == 1000;
	bool nlt = a >= 1000, ngt = a <= 1000, neq = a != 1000;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", a, 1000, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpu1000r(char b, bool lt, bool gt, bool eq)
{
	bool clt = 1000< b, cgt = 1000 > b, ceq = 1000 == b;
	bool nlt = 1000 >= b, ngt = 1000 <= b, neq = 1000 != b;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", 1000, b, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpum100(char a, bool lt, bool gt, bool eq)
{
	bool clt = a < -100, cgt = a > -100, ceq = a == -100;
	bool nlt = a >= -100, ngt = a <= -100, neq = a != -100;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", a, -100, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
}

void cmpum100r(char b, bool lt, bool gt, bool eq)
{
	bool clt = -100 < b, cgt = -100 > b, ceq = -100 == b;
	bool nlt = -100 >= b, ngt = -100 <= b, neq = -100 != b;

	printf("CPMPU %d, %d LT %d%d%d GT %d%d%d EQ %d%d%d\n", -100, b, lt, clt, nlt, gt, cgt, ngt, eq, ceq, neq);

	assert(clt == lt);
	assert(cgt == gt);
	assert(ceq == eq);

	assert(nlt != lt);
	assert(ngt != gt);
	assert(neq != eq);
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

	cmpuz(0);
	cmpuz(1);
	cmpuz(127);
	cmpuz(128);
	cmpuz(255);

	cmp1(0);
	cmp1(1);
	cmp1(2);
	cmp1(3);
	cmp1(127);
	cmp1(-1);
	cmp1(-2);
	cmp1(-3);
	cmp1(-128);

	cmpu(0, 0, false, false, true);
	cmpu(0, 1, true,  false, false);
	cmpu(1, 0, false ,true,  false);

	cmpu(128, 128, false, false, true);
	cmpu(  0, 128, true,  false, false);
	cmpu(128,   0, false ,true,  false);

	cmpu(255, 255, false, false, true);
	cmpu(  0, 255, true,  false, false);
	cmpu(255,   0, false ,true,  false);

	cmpu(127, 127, false, false, true);
	cmpu(128, 255, true,  false, false);
	cmpu(255, 128, false ,true,  false);

	cmpu100(100, false, false, true);
	cmpu100(  0, true,  false, false);
	cmpu100(130, false, true, false);

	cmpu100r(100, false, false, true);
	cmpu100r(130, true,  false, false);
	cmpu100r(  0, false, true, false);

	cmpum100(  0, false, true, false);
	cmpum100(130, false, true, false);

	cmpum100r(130, true,  false, false);
	cmpum100r(  0, true,  false, false);

	cmpu1000(100, true,  false, false);
	cmpu1000(  0, true,  false, false);
	cmpu1000(130, true,  false, false);

	cmpu1000r(100, false, true, false);
	cmpu1000r(130, false, true, false);
	cmpu1000r(  0, false, true, false);


	return 0;	
}
