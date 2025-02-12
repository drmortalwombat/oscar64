#include "Compression.h"

int CompressLZO(uint8* dst, const uint8* source, int size)
{
	int		csize = 0;

	int	pos = 0;
	while (pos < size)
	{
		int	pi = 0;
		while (pi < 127 && pos < size)
		{
			int	bi = pi, bj = 0;
			for (int i = 1; i <= (pos < 255 ? pos : 255); i++)
			{
				int j = 0;
				while (j < 127 && pos + j < size && source[pos - i + j] == source[pos + j])
					j++;

				if (j > bj)
				{
					bi = i;
					bj = j;
				}
			}

			if (bj >= 4)
			{
				if (pi > 0)
				{
					dst[csize++] = pi;
					for (int i = 0; i < pi; i++)
						dst[csize++] = source[pos - pi + i];
					pi = 0;
				}

				dst[csize++] = 128 + bj;
				dst[csize++] = bi;
				pos += bj;
			}
			else
			{
				pos++;
				pi++;
			}
		}

		if (pi > 0)
		{
			dst[csize++] = pi;
			for (int i = 0; i < pi; i++)
				dst[csize++] = source[pos - pi + i];
		}
	}

	dst[csize++] = 0;

	return csize;
}
