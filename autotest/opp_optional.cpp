#include <opp/optional.h>
#include <opp/string.h>

#include <assert.h>
#include <opp/optional.h>
#include <opp/string.h>

using opp::optional;
using opp::string;

optional<int> isqrt(int i)
{
	if (i < 0)
		return optional<int>();
	else
		return i;
}

optional<string> ssqrt(int i)
{
	if (i < 0)
		return optional<string>();
	else
		return opp::to_string(i);
}

int main(void)
{
	int sum = 0;
	for(int i=-10; i<=10; i++)
	{
		for (auto s : isqrt(i))
			sum += s;
	}
	assert(sum == 55);

	sum = 0;
	for(int i=-10; i<=10; i++)
	{
		if (auto s = ssqrt(i))
			sum += s->to_int();
	}
	assert(sum == 55);

	sum = 0;
	for(int i=-10; i<=10; i++)
	{
		for (auto s : ssqrt(i))
			sum += s.to_int();
	}
	assert(sum == 55);

	sum = 0;
	for(int i=-10; i<=10; i++)
	{
		auto s = ssqrt(i);
		auto t = s;

		if (t)
			sum += t->to_int();
	}
	assert(sum == 55);
}
