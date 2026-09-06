#include <assert.h>

/* // Spaced line comment within block comment */
/*// Unspaced line comment immediately after block comment opener */
/***/
/****/
/* * / * / */
/* / */
/* // */
/* /* */

#define IDENTITY(value) (value)
#define ADD(a, b) ((a) + (b))
#define MULTI(a, b, c) ((a) + (b) + (c))

int main(void)
{
	int a = 1;

	/* // Spaced block comment with code inside that must be ignored
	a += 100;
	*/

	/*// Unspaced block comment with code inside that must be ignored
	a += 200;
	*/

	/*//
	a += 300;
	//*/

	/**/

	/***/

	/* / */

	/* * / */

	assert(a == 1);

	int m1 = IDENTITY(
		// Owner's value.
		10
	);
	assert(m1 == 10);

	int m2 = IDENTITY(
		// Owner value, retained.
		20
	);
	assert(m2 == 20);

	int m3 = IDENTITY(
		/* Owner's value with ' apostrophe and , comma */
		30
	);
	assert(m3 == 30);

	int m4 = IDENTITY(
		/*// Unspaced block comment in macro */
		40
	);
	assert(m4 == 40);

	int m5 = ADD(
		// first arg with ' and ,
		50,
		// second arg with " and )
		60
	);
	assert(m5 == 110);

	int m6 = MULTI(
		/* arg 1 */ 1,
		/* arg 2 */ 2,
		/* arg 3 */ 3
	);
	assert(m6 == 6);

	return 0;
}
