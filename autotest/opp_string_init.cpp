#include <opp/string.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using opp::string;


const string	s1;
const string	s2 = "Hello";
const string	s3{"World"};

const string	a1[2];
const string	a2[2] = {"Hello", "World"};
const string	a3[2] = {opp::string("Hello"), opp::string("World")};


const string	d1[3][2];
const string	d2[3][2] = {{"Hello", "World"}, {"aaa", "bbb"}, {"ccc", "ddd"}};
const string	d3[3][2] = 
	{{opp::string("Hello"), opp::string("World")},
	 {opp::string("aaa"), opp::string("bbb")},
	 {opp::string("ccc"), opp::string("ddd")}};

void test_global_init(void)
{
	assert(!strcmp(s1.tocstr(), ""));
	assert(!strcmp(s2.tocstr(), "Hello"));
	assert(!strcmp(s3.tocstr(), "World"));		
}


void test_global_ainit(void)
{
	assert(!strcmp(a1[0].tocstr(), ""));
	assert(!strcmp(a1[1].tocstr(), ""));

	assert(!strcmp(a2[0].tocstr(), "Hello"));
	assert(!strcmp(a2[1].tocstr(), "World"));		

	assert(!strcmp(a3[0].tocstr(), "Hello"));
	assert(!strcmp(a3[1].tocstr(), "World"));		
}

void test_global_dinit(void)
{
	assert(!strcmp(d1[0][0].tocstr(), ""));
	assert(!strcmp(d1[2][1].tocstr(), ""));

	assert(!strcmp(d2[0][0].tocstr(), "Hello"));
	assert(!strcmp(d2[2][1].tocstr(), "ddd"));

	assert(!strcmp(d3[0][0].tocstr(), "Hello"));
	assert(!strcmp(d3[2][1].tocstr(), "ddd"));		
}


void test_local_init(void)
{
	const string	s1;
	const string	s2 = "Hello";
	const string	s3{"World"};

	assert(!strcmp(s1.tocstr(), ""));
	assert(!strcmp(s2.tocstr(), "Hello"));
	assert(!strcmp(s3.tocstr(), "World"));		
}


void test_local_ainit(void)
{
	const string	a1[2];
	const string	a2[2] = {"Hello", "World"};
	const string	a3[2] = {opp::string("Hello"), opp::string("World")};

	assert(!strcmp(a1[0].tocstr(), ""));
	assert(!strcmp(a1[1].tocstr(), ""));

	assert(!strcmp(a2[0].tocstr(), "Hello"));
	assert(!strcmp(a2[1].tocstr(), "World"));		

	assert(!strcmp(a3[0].tocstr(), "Hello"));
	assert(!strcmp(a3[1].tocstr(), "World"));		
}

void test_local_dinit(void)
{
	const string	d1[3][2];
	const string	d2[3][2] = {{"Hello", "World"}, {"aaa", "bbb"}, {"ccc", "ddd"}};
	const string	d3[3][2] = 
		{{opp::string("Hello"), opp::string("World")},
		 {opp::string("aaa"), opp::string("bbb")},
		 {opp::string("ccc"), opp::string("ddd")}};

	assert(!strcmp(d1[0][0].tocstr(), ""));
	assert(!strcmp(d1[2][1].tocstr(), ""));

	assert(!strcmp(d2[0][0].tocstr(), "Hello"));
	assert(!strcmp(d2[2][1].tocstr(), "ddd"));

	assert(!strcmp(d3[0][0].tocstr(), "Hello"));
	assert(!strcmp(d3[2][1].tocstr(), "ddd"));		
}

class X
{
public:
	const string s1;
	const string s2 = "Hello";
	const string s3;

	const string	a1[2];
	const string	a2[2] = {"Hello", "World"};

	const string	d1[3][2];
	const string	d2[3][2] = {{"Hello", "World"}, {"aaa", "bbb"}, {"ccc", "ddd"}};

	X() : s3("World") {}
};

void test_member_init(void)
{
	X x;

	assert(!strcmp(x.s1.tocstr(), ""));
	assert(!strcmp(x.s2.tocstr(), "Hello"));
	assert(!strcmp(x.s3.tocstr(), "World"));		
}

void test_member_ainit(void)
{
	X x;

	assert(!strcmp(x.a1[0].tocstr(), ""));
	assert(!strcmp(x.a1[1].tocstr(), ""));

	assert(!strcmp(x.a2[0].tocstr(), "Hello"));
	assert(!strcmp(x.a2[1].tocstr(), "World"));		
}

void test_member_dinit(void)
{
	X x;

	assert(!strcmp(x.d1[0][0].tocstr(), ""));
	assert(!strcmp(x.d1[2][1].tocstr(), ""));

	assert(!strcmp(x.d2[0][0].tocstr(), "Hello"));
	assert(!strcmp(x.d2[2][1].tocstr(), "ddd"));
}

void test_copy_init(void)
{
	X x;
	X y(x);

	assert(!strcmp(y.s1.tocstr(), ""));
	assert(!strcmp(y.s2.tocstr(), "Hello"));
	assert(!strcmp(y.s3.tocstr(), "World"));		
}

void test_copy_ainit(void)
{
	X x;
	X y(x);

	assert(!strcmp(y.a1[0].tocstr(), ""));
	assert(!strcmp(y.a1[1].tocstr(), ""));

	assert(!strcmp(y.a2[0].tocstr(), "Hello"));
	assert(!strcmp(y.a2[1].tocstr(), "World"));		
}

int main(void)
{
	test_global_init();
	test_global_ainit();
	test_global_dinit();

	for(int i=0; i<10000; i++)
	{
		test_local_init();
		test_local_ainit();
	}

	test_member_init();
	test_member_ainit();

	test_copy_init();
	test_copy_ainit();

	return 0;
}