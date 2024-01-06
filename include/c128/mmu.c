#include "mmu.h"

inline char mmu_set(char cr)
{
	char pcr = mmu.cr;
	mmu.cr = cr;
	return pcr;	
}
