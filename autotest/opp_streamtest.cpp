#include <opp/string.h>
#include <stdlib.h>
#include <assert.h>
#include <opp/sstream.h>
#include <math.h>

float fdist(float a, float b)
{
	float d = fabs(a - b);
	a = fabs(a);
	b = fabs(b);
	return d / (a > b ? a : b);
}

int main(void)
{
	ostringstream	os;

	for(int i=0; i<40; i++)
	{
		os << i << endl;
	}

	costream	cout;

	istringstream	is(os.str());

	int j = 0, k = 47;
#if 1

	while (is >> k)
	{
		assert(k == j++);
	}

	assert(j == 40);
#endif
	os.str(string());

#if 0
	cout << "[" << os.str() << "]" << endl;
	os << "HELLO";
	cout << "[" << os.str() << "]" << endl;
#endif
#if 1

	float	f = 1.0, g = 1.0;

	for(int i=0; i<10; i++)
	{
		os << f << " " << g << endl;
//		cout << os.str();

		f *= 5.1;
		g *= 0.12;
	}


	is.str(os.str());

	f = 1.0, g = 1.0;

	float	 fr, gr;

	j = 0;
	while (is >> fr >> gr)
	{
//		cout << f << " " << fr << ", " << g << " " << gr << ", " << fdist(fr, f) << endl;

		assert(fdist(fr, f) < 1.0e-5);
		assert(fdist(gr, g) < 1.0e-5);

		f *= 5.1;
		g *= 0.12;
		j++;
	}

	assert(j == 10);
#endif
	return 0;
}
