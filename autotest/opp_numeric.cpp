#include <assert.h>
#include <opp/numeric.h>
#include <opp/string.h>
#include <opp/iterator.h>
#include <opp/array.h>
#include <opp/sstream.h>
#include <opp/utility.h>

using opp::string, opp::cout;

int main(void)
{
	opp::array<int, 10>	v;

	opp::iota(v.begin(), v.end(), 10);	

	string	s = opp::accumulate(
		opp::next(v.begin()), v.end(), opp::to_string(v[0]), 
		[](const opp::string & ws, int k) {return ws + "-" + opp::to_string(k);});

	assert(s == "10-11-12-13-14-15-16-17-18-19");	

	opp::ostringstream	osstr;
	opp::inclusive_scan(v.begin(), v.end(), opp::ostream_iterator<int>(osstr, ", "));
	assert(osstr.str() == "10, 21, 33, 46, 60, 75, 91, 108, 126, 145, ");

	osstr.str("");
	opp::exclusive_scan(v.begin(), v.end(), opp::ostream_iterator<int>(osstr, ", "), 0);
	assert(osstr.str() == "0, 10, 21, 33, 46, 60, 75, 91, 108, 126, ");

	opp::array<int, 10>	w;

	opp::iota(w.begin(), w.end(), 1);

	assert(opp::inner_product(v.begin(), v.end(), w.begin(), 0) == 880);
}