#include <opp/string.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using opp::string;

static const char HelloWorld[] = "Hello World";
static const char AndBeyond[] = "And Beyond";
static const char And[] = "And";
static const char HelloWorldAndBeyond[] = "Hello World And Beyond";

__noinline void test_create(void)
{
	string	s1();
	string	s2(HelloWorld);
	string	s3(s2);
	string	s4('a');

	assert(!strcmp(s2.tocstr(), HelloWorld));
	assert(!strcmp(s3.tocstr(), HelloWorld));
	assert(s4.size() == 1 && s4[0] == 'a');
}

__noinline void test_concat(void)
{
	string	s1();
	string	s2(HelloWorld);
	string	s3(AndBeyond);

	string	s4 = s1 + s2;
	string	s5 = s2 + " " + s3;
	string	s6 = s2 + " " + AndBeyond;

	assert(!strcmp(s4.tocstr(), HelloWorld));
	assert(!strcmp(s5.tocstr(), HelloWorldAndBeyond));
	assert(!strcmp(s6.tocstr(), HelloWorldAndBeyond));	
}

__noinline void test_find(void)
{
	string s1(HelloWorldAndBeyond);
	string s2(And);

	assert(s1.find(HelloWorld) == 0);
	assert(s1.find(AndBeyond) == 12);
	assert(s1.find(And) == 12);
	assert(s1.find(s2) == 12);

	assert(s1.find(' ') == 5);
	assert(s1.find(' ', 6) == 11);
}

__noinline void test_assign(void)
{
	string	s1(HelloWorld);
	string	s2(AndBeyond);
	string	s3;
	s3 = s1;
	s3 = s2;
	s3 = s1;
	s3 += " ";
	s3 += s2;

	assert(!strcmp(s3.tocstr(), HelloWorldAndBeyond));

	s3 <<= 12;

	assert(!strcmp(s3.tocstr(), AndBeyond));

	s3 = HelloWorldAndBeyond;

	assert(!strcmp(s3.tocstr(), HelloWorldAndBeyond));

	s3 >>= 11;

	assert(!strcmp(s3.tocstr(), HelloWorld));
}

static char * test;

int main(void)
{
	test = new char;		

	unsigned	avail = heapfree();

	test_create();
	assert(avail == heapfree());

	test_concat();
	assert(avail == heapfree());

	test_find();
	assert(avail == heapfree());

	test_assign();
	assert(avail == heapfree());

	delete test;

	return 0;
}
