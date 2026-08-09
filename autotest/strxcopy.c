#include <assert.h>

static char * const Screen1 = (char *)0xc400;
static char * const ItemScreen = Screen1;

const char strxcopy(const char * str, char x)
{
	char i = 0;
	while ((char c = str[i++]))
		ItemScreen[42 + x++] = c | 0xc0;
	ItemScreen[42 + x++] = ' ' | 0xc0;
	return x;
}

int main(void)
{
	char x = strxcopy("hello", 0);
	x = strxcopy(" ", x);
	x = strxcopy("word",x);
	x = strxcopy(" ", x);

	assert(x == 6 + 2 + 5 + 2);
	assert(Screen1[42 +  0] == ('h' | 0xc0));
	assert(Screen1[42 +  4] == ('o' | 0xc0));
	assert(Screen1[42 +  5] == (' ' | 0xc0));
	assert(Screen1[42 +  8] == ('w' | 0xc0));
	assert(Screen1[42 + 11] == ('d' | 0xc0));
	assert(Screen1[42 + 12] == (' ' | 0xc0));

	return 0;
}
