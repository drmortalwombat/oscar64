#include <assert.h>

struct Pair
{
	char first, second;
};

static int make_count;

static struct Pair make_pair(char first, char second)
{
	struct Pair pair = { first, second };
	make_count++;
	return pair;
}

static struct Pair choose_values(int choose_left, struct Pair left, struct Pair right)
{
	return choose_left ? left : right;
}

static struct Pair choose_calls(int choose_left)
{
	struct Pair pair = choose_left ? make_pair(1, 2) : make_pair(3, 4);
	return pair;
}

static char choose_values_first(int choose_left, struct Pair left, struct Pair right)
{
	return (choose_left ? left : right).first;
}

static char choose_values_second(int choose_left, struct Pair left, struct Pair right)
{
	return (choose_left ? left : right).second;
}

static struct Pair choose_calls_return(int choose_left)
{
	return choose_left ? make_pair(1, 2) : make_pair(3, 4);
}

static char choose_calls_first(int choose_left)
{
	return (choose_left ? make_pair(1, 2) : make_pair(3, 4)).first;
}

static char choose_calls_second(int choose_left)
{
	return (choose_left ? make_pair(1, 2) : make_pair(3, 4)).second;
}


static struct Pair assign_calls(int choose_left)
{
	struct Pair pair;

	if (choose_left)
		pair = make_pair(5, 6);
	else
		pair = make_pair(7, 8);
	return pair;
}

typedef unsigned long		u32;

struct BigPair
{
	u32 first, second;
};

static int make_count_big;

static struct BigPair make_pair_big(u32 first, u32 second)
{
	struct BigPair pair = { first, second };
	make_count_big++;
	return pair;
}

static struct BigPair choose_values_big(int choose_left, struct BigPair left, struct BigPair right)
{
	return choose_left ? left : right;
}

static u32 choose_values_first_big(int choose_left, struct BigPair left, struct BigPair right)
{
	return (choose_left ? left : right).first;
}

static u32 choose_values_second_big(int choose_left, struct BigPair left, struct BigPair right)
{
	return (choose_left ? left : right).second;
}

static struct BigPair choose_calls_big(int choose_left)
{
	struct BigPair pair = choose_left ? make_pair_big(1, 2) : make_pair_big(3, 4);
	return pair;
}

static struct BigPair choose_calls_return_big(int choose_left)
{
	return choose_left ? make_pair_big(1, 2) : make_pair_big(3, 4);
}

static u32 choose_calls_first_big(int choose_left)
{
	return (choose_left ? make_pair_big(1, 2) : make_pair_big(3, 4)).first;
}

static u32 choose_calls_second_big(int choose_left)
{
	return (choose_left ? make_pair_big(1, 2) : make_pair_big(3, 4)).second;
}

static struct BigPair assign_calls_big(int choose_left)
{
	struct BigPair pair;

	if (choose_left)
		pair = make_pair_big(5, 6);
	else
		pair = make_pair_big(7, 8);
	return pair;
}

__noinline void test_pair(void)
{
	struct Pair left = { 9, 10 }, right = { 11, 12 };
	struct Pair a = choose_values(0, left, right);
	struct Pair b = choose_calls(1);
	struct Pair c = assign_calls(0);
	struct Pair d = choose_calls_return(1);

	assert(a.first == 11);
	assert(a.second == 12);
	assert(b.first == 1);
	assert(b.second == 2);
	assert(c.first == 7);
	assert(c.second == 8);
	assert(d.first == 1);
	assert(d.second == 2);

	assert(choose_values_first(0, left, right) == 11);
	assert(choose_values_second(0, left, right) == 12);

	assert(choose_calls_first(0) == 3);
	assert(choose_calls_second(0) == 4);
	assert(choose_calls_first(1) == 1);
	assert(choose_calls_second(1) == 2);

	assert(make_count == 7);	
}

__noinline void test_pair_big(void)
{
	struct BigPair left = { 9, 10 }, right = { 11, 12 };
	struct BigPair a = choose_values_big(0, left, right);
	struct BigPair b = choose_calls_big(1);
	struct BigPair c = assign_calls_big(0);
	struct BigPair d = choose_calls_return_big(1);

	assert(a.first == 11);
	assert(a.second == 12);
	assert(b.first == 1);
	assert(b.second == 2);
	assert(c.first == 7);
	assert(c.second == 8);
	assert(d.first == 1);
	assert(d.second == 2);

	assert(choose_values_first_big(0, left, right) == 11);
	assert(choose_values_second_big(0, left, right) == 12);

	assert(choose_calls_first_big(0) == 3);
	assert(choose_calls_second_big(0) == 4);
	assert(choose_calls_first_big(1) == 1);
	assert(choose_calls_second_big(1) == 2);

	assert(make_count_big == 7);	
}

int main(void)
{
	test_pair();
	test_pair_big();

	return 0;
}
