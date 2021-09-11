#include <stdio.h>

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

int main(void)
{
	for(int i=0; i<32; i++)
	{
		printf("1: %.4x : %.4x | %.4x : %.4x\n", shl1b(i), shl1n(i), shr1b(i), shr1n(i));
	}

	for(int i=0; i<32; i++)
	{
		printf("4: %.4x : %.4x | %.4x : %.4x\n", shl4b(i), shl4n(i), shr4b(i), shr4n(i));
	}

	for(int i=0; i<32; i++)
	{
		printf("8: %.4x : %.4x | %.4x : %.4x\n", shl8b(i), shl8n(i), shr8b(i), shr8n(i));
	}

	return 0;
}
