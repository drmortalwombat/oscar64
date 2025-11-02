#include <opp/string.h>
#include <opp/hashmap.h>
#include <assert.h>
#include <stdio.h>

void test_int_int(void)
{
	opp::hashmap<int, long>	hm;
	for(int i=0; i<100; i++)
		hm.at(i) = (long)i * (long)i;

	int sum = 0;
	for(auto [k, v] : hm)
	{
		sum += k;
		assert(v == (long)k * (long)k);
	}
	assert(sum == 4950);

	auto it = hm.begin();
	while (it != hm.end())
	{
		if (it->key & 1)
			it = hm.erase(it);
		else
			it++;
	}

	sum = 0;
	for(auto [k, v] : hm)
	{
		sum += k;
		assert(v == (long)k * (long)k);
	}
	assert(sum == 2450);

	sum = 0;
	for(int i=0; i<100; i++)
	{
		auto it = hm.find(i);
		if (it != hm.end())
		{
			assert(it->value == (long)it->key * (long)it->key);
			sum += it->key;
		}
	}
	assert(sum == 2450);
}

void test_string_string(void)
{
	opp::hashmap<opp::string, opp::string>	hm;
	for(int i=0; i<100; i++)
		hm.at(opp::to_string(i)) = opp::to_string((long)i * (long)i);

	int sum = 0;
	for(auto [k, v] : hm)
	{
		long l = v.to_long();
		int i = k.to_int();
		sum += i;
		assert(l == (long)i * (long)i);
	}
	assert(sum == 4950);

	auto it = hm.begin();
	while (it != hm.end())
	{
		long i = it->key.to_int();
		if (i & 1)
			it = hm.erase(it);
		else
			it++;
	}

	sum = 0;
	for(auto [k, v] : hm)
	{
		long l = v.to_long();
		long i = k.to_int();
		sum += i;
		assert(l == (long)i * (long)i);
	}
	assert(sum == 2450);

	sum = 0;
	for(int i=0; i<100; i++)
	{
		auto it = hm.find(opp::to_string(i));
		if (it != hm.end())
		{
			long l = it->value.to_long();
			long k = it->key.to_int();
			assert(l == (long)k * (long)k);
			sum += k;
		}
	}
	assert(sum == 2450);
}

void test_int_string(void)
{
	opp::hashmap<int, opp::string>	hm;
	for(int i=0; i<100; i++)
		hm.at(i) = opp::to_string((long)i * (long)i);

	int sum = 0;
	for(auto [k, v] : hm)
	{
		sum += k;
		long l = v.to_long();
		assert(l == (long)k * (long)k);
	}
	assert(sum == 4950);

	auto it = hm.begin();
	while (it != hm.end())
	{
		if (it->key & 1)
			it = hm.erase(it);
		else
			it++;
	}

	sum = 0;
	for(auto [k, v] : hm)
	{
		sum += k;
		long l = v.to_long();
		assert(l == (long)k * (long)k);
	}
	assert(sum == 2450);

	sum = 0;
	for(int i=0; i<100; i++)
	{
		auto it = hm.find(i);
		if (it != hm.end())
		{
			long l = it->value.to_long();
			assert(l == (long)it->key * (long)it->key);
			sum += it->key;
		}
	}
	assert(sum == 2450);
}

int main(void)
{
	test_int_int();
	test_int_string();
	test_string_string();
}
