#include <stdio.h>
#include <assert.h>

#pragma region( main, 0x0a00, 0xd000, , , {code, data, bss, heap, stack} )

unsigned shl1b(int n)
{
	return 1 << n;
}

unsigned shl1n(int n)
{
	return 1 << n;
}

#pragma native(shl1n)

unsigned shr1b(int n)
{
	return 0x8000 >> n;
}

unsigned shr1n(int n)
{
	return 0x8000 >> n;
}

#pragma native(shr1n)

unsigned shl4b(int n)
{
	return 0x0010 << n;
}

unsigned shl4n(int n)
{
	return 0x0010 << n;
}

#pragma native(shl4n)

unsigned shr4b(int n)
{
	return 0x0800 >> n;
}

unsigned shr4n(int n)
{
	return 0x0800 >> n;
}

#pragma native(shr4n)


unsigned shl8b(int n)
{
	return 0x0100 << n;
}

unsigned shl8n(int n)
{
	return 0x0100 << n;
}

#pragma native(shl8n)

unsigned shr8b(int n)
{
	return 0x0080 >> n;
}

unsigned shr8n(int n)
{
	return 0x0080 >> n;
}

#pragma native(shr8n)


void shl8xb(unsigned char xu, signed char xi)
{
	unsigned char 	ua[16];
	signed char 	ia[16];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == (unsigned char)(xu << i));
		assert(ia[i] == (signed char)(xi << i));
	}
}

void shr8xb(unsigned char xu, signed char xi)
{
	unsigned char	ua[16];
	signed char		ia[16];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == (unsigned char)(xu >> i));
		assert(ia[i] == (signed char)(xi >> i));
	}
}

void shl8xn(unsigned char xu, signed char xi)
{
	unsigned char	ua[16];
	signed char	    ia[16];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == (unsigned char)(xu << i));
		assert(ia[i] == (signed char)(xi << i));
	}
}

void shr8xn(unsigned char xu, signed char xi)
{
	unsigned char	ua[16];
	signed char		ia[16];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == (unsigned char)(xu >> i));
		assert(ia[i] == (signed char)(xi >> i));
	}
}

#pragma native(shl8xn)
#pragma native(shr8xn)




void shl16b(unsigned xu, int xi)
{
	unsigned	ua[16];
	int			ia[16];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == xu << i);
		assert(ia[i] == xi << i);
	}
}

void shr16b(unsigned xu, int xi)
{
	unsigned	ua[16];
	int			ia[16];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == xu >> i);
		assert(ia[i] == xi >> i);
	}
}

void shl16n(unsigned xu, int xi)
{
	unsigned	ua[16];
	int			ia[16];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == xu << i);
		assert(ia[i] == xi << i);
	}
}

void shr16n(unsigned xu, int xi)
{
	unsigned	ua[16];
	int			ia[16];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 16	

	for(int i=0; i<16; i++)
	{
		assert(ua[i] == xu >> i);
		assert(ia[i] == xi >> i);
	}
}

#pragma native(shl16n)
#pragma native(shr16n)


void shl32b(unsigned long xu, long xi)
{
	unsigned long	ua[32];
	long			ia[32];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 32

	for(int i=0; i<32; i++)
	{
		assert(ua[i] == xu << i);
		assert(ia[i] == xi << i);
	}
}

void shr32b(unsigned long xu, long xi)
{
	unsigned long	ua[32];
	long			ia[32];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 32

	for(int i=0; i<32; i++)
	{
		assert(ua[i] == xu >> i);
		assert(ia[i] == xi >> i);
	}
}

void shl32n(unsigned long xu, long xi)
{
	unsigned long	ua[32];
	long			ia[32];
#assign s 0
#repeat
	ua[s] = xu << s;
	ia[s] = xi << s;
#assign s s + 1
#until s == 32	

	for(int i=0; i<32; i++)
	{
		assert(ua[i] == xu << i);
		assert(ia[i] == xi << i);
	}
}

void shr32n(unsigned long xu, long xi)
{
	unsigned long	ua[32];
	long			ia[32];
#assign s 0
#repeat
	ua[s] = xu >> s;
	ia[s] = xi >> s;
#assign s s + 1
#until s == 32

	for(int i=0; i<32; i++)
	{
		assert(ua[i] == xu >> i);
		assert(ia[i] == xi >> i);
	}
}

void shl1_32n(void)
{
	static const unsigned long m[] = {
		#for(i, 32) 1ul << i,
	};

	for(int i=0; i<32; i++)
	{
		assert(1ul << i == m[i]);
	}
}

#pragma native(shl32n)
#pragma native(shr32n)

int main(void)
{
	*(volatile char *)0x01 = 0x36;

	for(int i=0; i<32; i++)
	{
		printf("1: %.4x : %.4x | %.4x : %.4x\n", shl1b(i), shl1n(i), shr1b(i), shr1n(i));
		assert(shl1b(i) == shl1n(i));
		assert(shr1b(i) == shr1n(i));
	}

	for(int i=0; i<32; i++)
	{
		printf("4: %.4x : %.4x | %.4x : %.4x\n", shl4b(i), shl4n(i), shr4b(i), shr4n(i));
		assert(shl4b(i) == shl4n(i));
		assert(shr4b(i) == shr4n(i));
	}

	for(int i=0; i<32; i++)
	{
		printf("8: %.4x : %.4x | %.4x : %.4x\n", shl8b(i), shl8n(i), shr8b(i), shr8n(i));
		assert(shl8b(i) == shl8n(i));
		assert(shr8b(i) == shr8n(i));
	}

	shl8xb(0x00, 0x00);
	shl8xb(0xff, 0xff);
	shl8xb(0x34, 0x34);
	shl8xb(0xdc, 0xdc);

	shr8xb(0x00, 0x00);
	shr8xb(0xff, 0xff);
	shr8xb(0x34, 0x34);
	shr8xb(0xdc, 0xdc);

	shl8xn(0x00, 0x00);
	shl8xn(0xff, 0xff);
	shl8xn(0x34, 0x34);
	shl8xn(0xdc, 0xdc);

	shr8xn(0x00, 0x00);
	shr8xn(0xff, 0xff);
	shr8xn(0x34, 0x34);
	shr8xn(0xdc, 0xdc);

	shl16b(0x0000, 0x0000);
	shl16b(0xffff, 0xffff);
	shl16b(0x1234, 0x1234);
	shl16b(0xfedc, 0xfedc);

	shr16b(0x0000, 0x0000);
	shr16b(0xffff, 0xffff);
	shr16b(0x1234, 0x1234);
	shr16b(0xfedc, 0xfedc);

	shl16n(0x0000, 0x0000);

	shl16n(0xffff, 0xffff);
	shl16n(0x1234, 0x1234);
	shl16n(0xfedc, 0xfedc);

	shr16n(0x0000, 0x0000);
	shr16n(0xffff, 0xffff);
	shr16n(0x1234, 0x1234);
	shr16n(0xfedc, 0xfedc);

	shl32b(0x00000000UL, 0x00000000L);
	shl32b(0x00000001UL, 0x00000001L);
	shl32b(0xffffffffUL, 0xffffffffL);
	shl32b(0x12345678UL, 0x12345678L);
	shl32b(0xfedcba98UL, 0xfedcba98L);

	shr32b(0x00000000UL, 0x00000000L);
	shr32b(0x00000001UL, 0x00000001L);
	shr32b(0xffffffffUL, 0xffffffffL);
	shr32b(0x12345678UL, 0x12345678L);
	shr32b(0xfedcba98UL, 0xfedcba98L);

	shl32n(0x00000000UL, 0x00000000L);
	shl32n(0x00000001UL, 0x00000001L);
	shl32n(0xffffffffUL, 0xffffffffL);
	shl32n(0x12345678UL, 0x12345678L);
	shl32n(0xfedcba98UL, 0xfedcba98L);

	shr32n(0x00000000UL, 0x00000000L);
	shr32n(0x00000001UL, 0x00000001L);
	shr32n(0xffffffffUL, 0xffffffffL);
	shr32n(0x12345678UL, 0x12345678L);
	shr32n(0xfedcba98UL, 0xfedcba98L);

	shl1_32n();

	return 0;
}
