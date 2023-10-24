#include <gfx/vector3d.h>
#include <assert.h>
#include <stdio.h>

Matrix4	ml, mr;

int main(void)
{
	for(char i=0; i<16; i++)
	{
		for(char j=0; j<16; j++)
		{
			for(char k=0; k<16; k++)
			{
				ml.m[k] = (i == k) ? 1.0 : 0.0;
				mr.m[k] = (j == k) ? 1.0 : 0.0;
			}

			mat4_mmul(&ml, &mr);

#if 0
			printf("%d, %d\n", i, j);
			for(char k=0; k<16; k++)
				printf("%f ", ml.m[k]);
			printf("\n");
#endif

			for(char k=0; k<16; k++)
			{
				char ix = i & 3, iy = i >> 2;
				char jx = j & 3, jy = j >> 2;
				char kx = k & 3, ky = k >> 2;

				if (ky == jy && kx == ix && jx == iy)
					assert(ml.m[k] == 1.0);
				else
					assert(ml.m[k] == 0.0);
			}
		}
	}
		
	return 0;
}
