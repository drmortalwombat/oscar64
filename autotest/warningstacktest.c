// Diagnostic selectors are checked by the warningstackconstraints target.
#if defined(WARNING_STACK_DEFAULT)
#pragma warning(push)
#pragma warning(disable: 2017,2002)
enum { suppressed = 40000 };
#pragma warning_stack_suppressed
#pragma warning(pop)
enum { restored = 40000 };
#pragma warning_stack_restored

#elif defined(WARNING_STACK_ERROR)
#pragma warning(error: 2017,2002)
#pragma warning(push)
#pragma warning(disable: 2017,2002)
enum { suppressed = 40000 };
#pragma warning_stack_suppressed
#pragma warning(pop)
enum { restored = 40000 };
#pragma warning_stack_restored

#elif defined(WARNING_STACK_INCLUDE)
#pragma warning(error: 2017,2002)
#include "warningstacktest.h"
enum { suppressed = 40000 };
#pragma warning_stack_suppressed
#pragma warning(pop)
enum { restored = 40000 };
#pragma warning_stack_restored

#elif defined(WARNING_STACK_NESTED) || defined(WARNING_STACK_NESTED_ERROR)
#pragma warning(error: 2017)
#pragma warning(disable: 2002)
#pragma warning(push)
#pragma warning(default: 2017)
#pragma warning(error: 2002)
#pragma warning(push)
#pragma warning(disable: 2017)
#pragma warning(default: 2002)
enum { suppressed = 40000 };
#pragma warning_stack_inner_default
#pragma warning(pop)
enum { middleDefault = 40000 };
#ifdef WARNING_STACK_NESTED_ERROR
#pragma warning_stack_middle_error
#endif
#pragma warning(pop)
#pragma warning_stack_outer_disabled
#ifdef WARNING_STACK_NESTED_ERROR
enum { outerError = 40000 };
#endif

#elif defined(WARNING_STACK_UNDERFLOW)
#pragma warning(disable: 2017,2002)
#pragma warning(pop)
enum { suppressed = 40000 };
#pragma warning_stack_suppressed

#else
// Restore disabled settings after nested changes to default and error levels.
#pragma warning(disable: 2017,2002)
#pragma warning(push)
#pragma warning(default: 2017,2002)
#pragma warning(push)
#pragma warning(error: 2017,2002)
#pragma warning(push)
#pragma warning(disable: 2017,2002)
#pragma warning(pop)
#pragma warning(pop)
#pragma warning(pop)
enum { suppressed = 40000 };
#pragma warning_stack_suppressed
#pragma warning(default: 2017,2002)
#endif

int main(void)
{
	return 0;
}
