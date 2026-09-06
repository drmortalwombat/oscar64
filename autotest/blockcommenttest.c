#include <assert.h>

/* // Spaced line comment within block comment */
/*// Unspaced line comment immediately after block comment opener */
/***/
/****/
/* * / * / */
/* / */
/* // */
/* /* */

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

	return 0;
}
