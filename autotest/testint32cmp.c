#include <stdio.h>
#include <assert.h>

bool beq(long a, long b)
{
	return a == b;
}

bool blt(long a, long b)
{
	return a < b;
}

bool bgt(long a, long b)
{
	return a > b;
}

bool ble(long a, long b)
{
	return a <= b;
}

bool bge(long a, long b)
{
	return a >= b;
}

bool neq(long a, long b)
{
	return a == b;
}

#pragma native(neq)

bool nlt(long a, long b)
{
	return a < b;
}

#pragma native(nlt)

bool ngt(long a, long b)
{
	return a > b;
}

#pragma native(ngt)

bool nle(long a, long b)
{
	return a <= b;
}

#pragma native(nle)

bool nge(long a, long b)
{
	return a >= b;
}

#pragma native(nge)




inline bool ieq(long a, long b)
{
	return a == b;
}

inline bool ilt(long a, long b)
{
	return a < b;
}

inline bool igt(long a, long b)
{
	return a > b;
}

inline bool ile(long a, long b)
{
	return a <= b;
}

inline bool ige(long a, long b)
{
	return a >= b;
}


bool beqz(long a)
{
	return a == 0;
}

bool bltz(long a)
{
	return a < 0;
}

bool bgtz(long a)
{
	return a > 0;
}

bool blez(long a)
{
	return a <= 0;
}

bool bgez(long a)
{
	return a >= 0;
}

bool neqz(long a)
{
	return a == 0;
}

#pragma native(neqz)

bool nltz(long a)
{
	return a < 0;
}

#pragma native(nltz)

bool ngtz(long a)
{
	return a > 0;
}

#pragma native(ngtz)

bool nlez(long a)
{
	return a <= 0;
}

#pragma native(nlez)

bool ngez(long a)
{
	return a >= 0;
}

#pragma native(ngez)




bool beq1(long a)
{
	return a == 1;
}

bool blt1(long a)
{
	return a < 1;
}

bool bgt1(long a)
{
	return a > 1;
}

bool ble1(long a)
{
	return a <= 1;
}

bool bge1(long a)
{
	return a >= 1;
}

bool neq1(long a)
{
	return a == 1;
}

#pragma native(neq1)

bool nlt1(long a)
{
	return a < 1;
}

#pragma native(nlt1)

bool ngt1(long a)
{
	return a > 1;
}

#pragma native(ngt1)

bool nle1(long a)
{
	return a <= 1;
}

#pragma native(nle1)

bool nge1(long a)
{
	return a >= 1;
}

#pragma native(nge1)



bool beqm(long a)
{
	return a == -1;
}

bool bltm(long a)
{
	return a < -1;
}

bool bgtm(long a)
{
	return a > -1;
}

bool blem(long a)
{
	return a <= -1;
}

bool bgem(long a)
{
	return a >= -1;
}



bool neqm(long a)
{
	return a == -1;
}

#pragma native(neqm)

bool nltm(long a)
{
	return a < -1;
}

#pragma native(nltm)

bool ngtm(long a)
{
	return a > -1;
}

#pragma native(ngtm)

bool nlem(long a)
{
	return a <= -1;
}

#pragma native(nlem)

bool ngem(long a)
{
	return a >= -1;
}

#pragma native(ngem)



void cmpc(long a, long b)
{
	bool	beqf = beq(a, b), bltf = blt(a, b), bgtf = bgt(a, b), blef = ble(a, b), bgef = bge(a, b);
	bool	neqf = neq(a, b), nltf = nlt(a, b), ngtf = ngt(a, b), nlef = nle(a, b), ngef = nge(a, b);
	
	printf("BYTE   %ld, %ld : EQ %d LT %d GT %d\r", a, b, beqf, bltf, bgtf);
	printf("NATIVE %ld, %ld : EQ %d LT %d GT %d\r", a, b, neqf, nltf, ngtf);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

void cmpi(long a, long b)
{
	bool	ieqf = ieq(a, b), iltf = ilt(a, b), igtf = igt(a, b), ilef = ile(a, b), igef = ige(a, b);
	bool	neqf = neq(a, b), nltf = nlt(a, b), ngtf = ngt(a, b), nlef = nle(a, b), ngef = nge(a, b);
	
	printf("INLINE %ld, %ld : EQ %d LT %d GT %d\r", a, b, ieqf, iltf, igtf);
	printf("NATIVE %ld, %ld : EQ %d LT %d GT %d\r", a, b, neqf, nltf, ngtf);
	
	assert(ieqf == neqf);
	assert(iltf == nltf);
	assert(igtf == ngtf);	
	assert(ilef == nlef);
	assert(igef == ngef);	
}

void cmp(long a, long b)
{
	cmpc(a, b);
	cmpi(a, b);
}

void cmpz(long a)
{
	bool	beqf = beqz(a), bltf = bltz(a), bgtf = bgtz(a), blef = blez(a), bgef = bgez(a);
	bool	neqf = neqz(a), nltf = nltz(a), ngtf = ngtz(a), nlef = nlez(a), ngef = ngez(a);
	
	printf("BYTE   %ld, 0 : EQ %d LT %d GT %d\r", a, beqf, bltf, bgtf);
	printf("NATIVE %ld, 0 : EQ %d LT %d GT %d\r", a, neqf, nltf, ngtf);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

void cmp1(long a)
{
	bool	beqf = beq1(a), bltf = blt1(a), bgtf = bgt1(a), blef = ble1(a), bgef = bge1(a);
	bool	neqf = neq1(a), nltf = nlt1(a), ngtf = ngt1(a), nlef = nle1(a), ngef = nge1(a);
	
	printf("BYTE   %ld, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, beqf, bltf, bgtf, blef, bgef);
	printf("NATIVE %ld, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, neqf, nltf, ngtf, nlef, ngef);
	
	assert(beqf == neqf);
	assert(bltf == nltf);
	assert(bgtf == ngtf);	
	assert(blef == nlef);
	assert(bgef == ngef);	
}

void cmpm(long a)
{
	bool	beqf = beqm(a), bltf = bltm(a), bgtf = bgtm(a), blef = blem(a), bgef = bgem(a);
	bool	neqf = neqm(a), nltf = nltm(a), ngtf = ngtm(a), nlef = nlem(a), ngef = ngem(a);
	
	printf("BYTE   %ld, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, beqf, bltf, bgtf, blef, bgef);
	printf("NATIVE %ld, 1 : EQ %d LT %d GT %d LE %d GE %d\r", a, neqf, nltf, ngtf, nlef, ngef);
	
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

	cmp( 10000000L, -10000000L);
	cmp( 10000000L, -20000000L);
	cmp( 20000000L, -10000000L);

	cmp(-10000000L,  10000000L);
	cmp(-10000000L,  20000000L);
	cmp(-20000000L,  10000000L);
	
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

	cmpz(0);
	cmpz(1);
	cmpz(255);
	cmpz(256);
	cmpz(10000);
	cmpz(20000);
	cmpz(-1);
	cmpz(-255);
	cmpz(-256);
	cmpz(-10000);
	cmpz(-20000);

	cmp1(0);
	cmp1(1);
	cmp1(2);
	cmp1(3);
	cmp1(255);
	cmp1(256);
	cmp1(10000);
	cmp1(20000);
	cmp1(1000000l);
	cmp1(2000000l);
	cmp1(100000000l);
	cmp1(200000000l);
	cmp1(-1);
	cmp1(-2);
	cmp1(-3);
	cmp1(-255);
	cmp1(-256);
	cmp1(-10000);
	cmp1(-20000);
	cmp1(-1000000l);
	cmp1(-2000000l);
	cmp1(-100000000l);
	cmp1(-200000000l);

	cmpm(0);
	cmpm(1);
	cmpm(2);
	cmpm(3);
	cmpm(255);
	cmpm(256);
	cmpm(10000);
	cmpm(20000);
	cmpm(1000000l);
	cmpm(2000000l);
	cmpm(100000000l);
	cmpm(200000000l);
	cmpm(-1);
	cmpm(-2);
	cmpm(-3);
	cmpm(-255);
	cmpm(-256);
	cmpm(-10000);
	cmpm(-20000);
	cmpm(-1000000l);
	cmpm(-2000000l);
	cmpm(-100000000l);
	cmpm(-200000000l);

	return 0;
	
}
