#include <opp/span.h>
#include <opp/iostream.h>
#include <opp/iterator.h>
#include <opp/vector.h>
#include <opp/array.h>
#include <opp/numeric.h>
#include <opp/algorithm.h>
#include <assert.h>

int main(void)
{
	static const int  test[] = {7, 6, 5, 4, 3, 2, 1};

	opp::span<const int, 7>	st(test);
	opp::span<const int>	sd(test, 7);

	auto	st1 = st.subspan<1>();
	auto	sd1 = sd.subspan<1>();

	auto	st2 = st.subspan(2);
	auto	sd2 = sd.subspan(2);

	auto	st3 = st.subspan<1, 4>();
	auto	sd3 = sd.subspan<1, 4>();

	auto	st4 = st.subspan(2, 5);
	auto	sd4 = sd.subspan(2, 5);

	auto	st5 = st.last(5);
	auto	sd5 = sd.last(5);

	assert(opp::accumulate(st.begin(), st.end(), 0) == 28);
	assert(opp::accumulate(sd.begin(), sd.end(), 0) == 28);

	assert(opp::accumulate(st1.begin(), st1.end(), 0) == 21);
	assert(opp::accumulate(sd1.begin(), sd1.end(), 0) == 21);

	assert(opp::accumulate(st2.begin(), st2.end(), 0) == 15);
	assert(opp::accumulate(sd2.begin(), sd2.end(), 0) == 15);

	assert(opp::accumulate(st3.begin(), st3.end(), 0) == 18);
	assert(opp::accumulate(sd3.begin(), sd3.end(), 0) == 18);

	assert(opp::accumulate(st4.begin(), st4.end(), 0) == 15);
	assert(opp::accumulate(sd4.begin(), sd4.end(), 0) == 15);

	assert(opp::accumulate(st5.begin(), st5.end(), 0) == 15);
	assert(opp::accumulate(sd5.begin(), sd5.end(), 0) == 15);
}