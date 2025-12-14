#ifndef OPP_NUMERIC_H
#define OPP_NUMERIC_H

namespace opp {

template<class InputIt, class T>
constexpr T accumulate( InputIt first, InputIt last, T init)
{
	for (; first != last; ++first)
		init = opp::move(init) + *first;
	return init;	
}

template<class InputIt, class T, class BinaryOperator>
constexpr T accumulate( InputIt first, InputIt last, T init, BinaryOperator op)
{
	for (; first != last; ++first)
		init = op(opp::move(init), *first);
	return init;	
}

template<class ForwardIt, class T>
constexpr void iota(ForwardIt first, ForwardIt last, T value)
{
	for (; first != last; ++first, ++value)
		*first = value;
}


template<class InputIt1, class InputIt2, class T>
constexpr T inner_product(InputIt1 first1, InputIt1 last1, InputIt2 first2, T init)
{
	while (first1 != last1)
	{
		init = opp::move(init) + (*first1) * (*first2);
		++first1;
		++first2;
	}

	return init;
}

template<class InputIt1, class InputIt2, class T, class BinaryOp1, class BinaryOp2>
constexpr T inner_product(InputIt1 first1, InputIt1 last1, InputIt2 first2, T init, BinaryOp1 op1, BinaryOp2 op2)
{
	while (first1 != last1)
	{
		init = op1(opp::move(init), op2(*first1, *first2));
		++first1;
		++first2;
	}

	return init;
}

template< class InputIt, class OutputIt, class T >
OutputIt exclusive_scan( InputIt first, InputIt last, OutputIt d_first, T init )
{
	while (first != last)
	{
		*d_first = init;
		init = opp::move(init) + *first;
		++d_first;
		++first;
	} 

	return d_first;
}

template< class InputIt, class OutputIt >
OutputIt inclusive_scan( InputIt first, InputIt last, OutputIt d_first )
{
	if (first == last)
		return d_first;

	auto sum = *first;
	*d_first = sum;

	while (++first != last)
	{
		sum = opp::move(sum) + *first;
		*++d_first = sum;
	}

	return ++d_first;
}

template< class InputIt, class OutputIt, class T, class BinaryOperator >
OutputIt exclusive_scan( InputIt first, InputIt last, OutputIt d_first, T init, BinaryOperator op )
{
	while (first != last)
	{
		*d_first = init;
		init = op(opp::move(init), *first);
		++d_first;
		++first;
	} 

	return d_first;
}

template< class InputIt, class OutputIt, class BinaryOperator >
OutputIt inclusive_scan( InputIt first, InputIt last, OutputIt d_first, BinaryOperator op )
{
	if (first == last)
		return d_first;

	auto sum = *first;
	*d_first = sum;

	while (++first != last)
	{
		sum = op(opp::move(sum), *first);
		*++d_first = sum;
	}

	return ++d_first;
}

}

#endif
