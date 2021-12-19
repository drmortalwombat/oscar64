#include <assert.h>

char a[200], b[200];
bool ok = true;

int main(void)
{
#assign ni 0
#repeat
	a[ni] = ni & 255;
#assign ni ni + 1
#until ni == 200

#assign ni 0
#repeat
	if (ok)
		b[ni] = ni & 255;
#assign ni ni + 1
#until ni == 200

	int asum = 0, bsum = 0, csum = 0;
	for(int i=0; i<200; i++)
	{
		asum += a[i];
		bsum += b[i];
		csum += i & 255;
	}
	
	return asum + bsum - 2 * csum;		
}
