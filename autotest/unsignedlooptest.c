#include <assert.h>

volatile int k0, k1;

void loop_down_gt_0_by_1(void)
{
	unsigned	k	= 437;
	while(k > 0)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_ge_0_by_1(void)
{
	unsigned	k	= 437;
	while(k >= 1)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_ne_0_by_1(void)
{
	unsigned	k	= 437;
	while(k != 0)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_gt_1_by_1(void)
{
	unsigned	k	= 438;
	while(k > 1)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_ge_1_by_1(void)
{
	unsigned	k	= 438;
	while(k >= 2)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_ne_1_by_1(void)
{
	unsigned	k	= 438;
	while(k != 1)
	{
		if (k & 1) k1++; else k0++;
		k--;
	}
}

void loop_down_gt_0_by_2(void)
{
	unsigned	k	= 438;
	while(k > 0)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}

void loop_down_ge_0_by_2(void)
{
	unsigned	k	= 438;
	while(k >= 1)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}

void loop_down_ne_0_by_2(void)
{
	unsigned	k	= 438;
	while(k != 0)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}

void loop_down_gt_1_by_2(void)
{
	unsigned	k	= 439;
	while(k > 1)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}

void loop_down_ge_1_by_2(void)
{
	unsigned	k	= 439;
	while(k >= 2)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}

void loop_down_ne_1_by_2(void)
{
	unsigned	k	= 439;
	while(k != 1)
	{
		if (k & 1) k1++; else k0++;
		k -= 2;
	}
}



void loop_up_lt_65535_by_1(void)
{
	unsigned	k	= 65535 - 437;
	while(k < 65535)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_le_65535_by_1(void)
{
	unsigned	k	= 65535 - 437;
	while(k <= 65534)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_ne_65535_by_1(void)
{
	unsigned	k	= 65535 - 437;
	while(k != 65535)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_lt_65534_by_1(void)
{
	unsigned	k	= 65535 - 438;
	while(k < 65534)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_le_65534_by_1(void)
{
	unsigned	k	= 65535 - 438;
	while(k <= 65533)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_ne_65534_by_1(void)
{
	unsigned	k	= 65535 - 438;
	while(k != 65534)
	{
		if (k & 1) k1++; else k0++;
		k++;
	}
}

void loop_up_lt_65535_by_2(void)
{
	unsigned	k	= 65535 - 438;
	while(k < 65535)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

void loop_up_le_65535_by_2(void)
{
	unsigned	k	= 65535 - 438;
	while(k <= 65534)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

void loop_up_ne_65535_by_2(void)
{
	unsigned	k	= 65535 - 438;
	while(k != 65535)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

void loop_up_lt_65534_by_2(void)
{
	unsigned	k	= 65535 - 439;
	while(k < 65534)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

void loop_up_le_65534_by_2(void)
{
	unsigned	k	= 65535 - 439;
	while(k <= 65533)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

void loop_up_ne_65534_by_2(void)
{
	unsigned	k	= 65535 - 439;
	while(k != 65534)
	{
		if (k & 1) k1++; else k0++;
		k += 2;
	}
}

int main(void)
{
	k0 = 0; k1 = 0; 
	loop_down_gt_0_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_down_ge_0_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_down_ne_0_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_down_gt_1_by_1();
	assert(k0 == 219 && k1 == 218);

	k0 = 0; k1 = 0; 
	loop_down_ge_1_by_1();
	assert(k0 == 219 && k1 == 218);

	k0 = 0; k1 = 0; 
	loop_down_ne_1_by_1();
	assert(k0 == 219 && k1 == 218);


	k0 = 0; k1 = 0; 
	loop_down_gt_0_by_2();
	assert(k0 == 219 && k1 == 0);

	k0 = 0; k1 = 0; 
	loop_down_ge_0_by_2();
	assert(k0 == 219 && k1 == 0);

	k0 = 0; k1 = 0; 
	loop_down_ne_0_by_2();
	assert(k0 == 219 && k1 == 0);

	k0 = 0; k1 = 0; 
	loop_down_gt_1_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_down_ge_1_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_down_ne_1_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_lt_65535_by_1();
	assert(k0 == 219 && k1 == 218);

	k0 = 0; k1 = 0; 
	loop_up_le_65535_by_1();
	assert(k0 == 219 && k1 == 218);

	k0 = 0; k1 = 0; 
	loop_up_ne_65535_by_1();
	assert(k0 == 219 && k1 == 218);

	k0 = 0; k1 = 0; 
	loop_up_lt_65534_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_le_65534_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_ne_65534_by_1();
	assert(k0 == 218 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_lt_65535_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_le_65535_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_ne_65535_by_2();
	assert(k0 == 0 && k1 == 219);

	k0 = 0; k1 = 0; 
	loop_up_lt_65534_by_2();
	assert(k0 == 219 && k1 == 0);

	k0 = 0; k1 = 0; 
	loop_up_le_65534_by_2();
	assert(k0 == 219 && k1 == 0);

	k0 = 0; k1 = 0; 
	loop_up_ne_65534_by_2();
	assert(k0 == 219 && k1 == 0);
}
