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

static struct Pair assign_calls(int choose_left)
{
	struct Pair pair;

	if (choose_left)
		pair = make_pair(5, 6);
	else
		pair = make_pair(7, 8);
	return pair;
}

int main(void)
{
	struct Pair left = { 9, 10 }, right = { 11, 12 };
	struct Pair a = choose_values(0, left, right);
	struct Pair b = choose_calls(1);
	struct Pair c = assign_calls(0);
	return (a.first - 11) | (a.second - 12)
		| (b.first - 1) | (b.second - 2)
		| (c.first - 7) | (c.second - 8)
		| (make_count - 2);
}
